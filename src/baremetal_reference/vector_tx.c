/* Integrated into the original ADC app with ENABLE_VECTOR_TX.
 * Boot is digitally muted. A command is required to start waveform playback.
 * No flash/SD writes. Baremetal memory map only; NOT safe to run under Linux.
 */
#ifdef ENABLE_VECTOR_TX
#include "vector_tx.h"
#include "tx_memory_map.h"
#include "xaxicdma.h"
#include "xaxidma.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "xil_mmu.h"
#include "xuartps_hw.h"
#include "sleep.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define UPLOAD_BASE 0xB0080000u
#define PLAY_BASE 0xB0090000u
#define CONTROL_BASE 0xB00A0000u
#define SOURCE ((UINTPTR)TX_SOURCE_ADDR)
#define READBACK ((UINTPTR)TX_READBACK_ADDR)
#define MAILBOX ((UINTPTR)TX_MAILBOX_ADDR)
#define PL_WAVE ((UINTPTR)0x000500100000ull)
#define MAX_BYTES TX_MAX_BYTES
#define TONE_SAMPLES 61440u /* exactly 512 periods at 10MHz / 1.2GS/s */
#define TONE_BYTES (TONE_SAMPLES*4u)
#define MAILBOX_MAGIC TX_MAILBOX_MAGIC
#define CHECK(call) do { int check_s=(call); if(check_s!=XST_SUCCESS) { \
    xil_printf("TX FAIL line %d: status=%d\r\n",__LINE__,check_s); return XST_FAILURE; } } while(0)
static XAxiCdma upload;
static XAxiDma playback;
static u8 descriptors[4096] __attribute__((aligned(64)));
static u32 loaded_bytes;
static int initialized;
/* A53-only application: order uncached RAM and mailbox ownership vs DMA MMIO. */
static void dma_barrier(void) {__asm__ __volatile__("dsb sy" ::: "memory");}

static u32 tx_status(void) {return Xil_In32(CONTROL_BASE+8);}
static u64 dac_nco_word(XRFdc *rf)
{
    u32 base=XRFDC_BLOCK_BASE(XRFDC_DAC_TILE,1,0);
    return ((u64)XRFdc_ReadReg16(rf,base,XRFDC_ADC_NCO_FQWD_UPP_OFFSET)<<32) |
           ((u64)XRFdc_ReadReg16(rf,base,XRFDC_ADC_NCO_FQWD_MID_OFFSET)<<16) |
           (u64)XRFdc_ReadReg16(rf,base,XRFDC_ADC_NCO_FQWD_LOW_OFFSET);
}
static int reset_playback(void)
{
    Xil_Out32(CONTROL_BASE,0); /* stop consuming; output becomes digital zeros */
    usleep(10);
    XAxiDma_Reset(&playback);
    unsigned timeout=100000;
    while(!XAxiDma_ResetIsDone(&playback) && --timeout) usleep(1);
    if(!timeout) return XST_FAILURE;
    Xil_Out32(CONTROL_BASE,2); /* flush FIFO and width converter */
    usleep(100);
    Xil_Out32(CONTROL_BASE,0);
    usleep(100);
    return XST_SUCCESS;
}
static int copy_dma(UINTPTR src, UINTPTR dst, u32 bytes)
{
    /* Never issue one oversized BTT. 4MiB is below both 23/26-bit limits. */
    for(u32 offset=0;offset<bytes;) {
        u32 chunk=bytes-offset;
        if(chunk>TX_COPY_CHUNK_BYTES) chunk=TX_COPY_CHUNK_BYTES;
        dma_barrier();
        CHECK(XAxiCdma_SimpleTransfer(&upload,src+offset,dst+offset,chunk,NULL,NULL));
        unsigned timeout=1000000;
        while(XAxiCdma_IsBusy(&upload) && --timeout) usleep(1);
        u32 errors=XAxiCdma_GetError(&upload);
        if(!timeout || errors) {
            xil_printf("TX FAIL CDMA offset=%d timeout=%d error=0x%08x\r\n",offset,!timeout,errors);
            return XST_FAILURE;
        }
        offset+=chunk;
    }
    dma_barrier();
    return XST_SUCCESS;
}
static int upload_wave(u32 bytes)
{
    if(!bytes || bytes>MAX_BYTES || (bytes&255u)) {
        xil_printf("TX reject: length must be 256-byte aligned and <=64MiB\r\n");
        return XST_FAILURE;
    }
    CHECK(reset_playback());
    /* Source/readback ranges are mapped noncacheable below. Descriptors remain
     * cached and are explicitly flushed before ownership is handed to DMA. */
    loaded_bytes=0;
    CHECK(copy_dma(SOURCE,PL_WAVE,bytes));
    CHECK(copy_dma(PL_WAVE,READBACK,bytes));
    if(memcmp((void *)SOURCE,(void *)READBACK,bytes)) {
        xil_printf("TX FAIL: PS->PL DDR->PS DMA round-trip differs\r\n");
        return XST_FAILURE;
    }
    loaded_bytes=bytes;
    xil_printf("TX DMA_ROUND_TRIP_PASS bytes=%d; loaded, NOT playing\r\n",bytes);
    return XST_SUCCESS;
}
static int make_tone(void)
{
    s16 *iq=(s16 *)SOURCE;
    for(unsigned n=0;n<TONE_SAMPLES;n++) {
        double phase=2.0*3.14159265358979323846*(n%120u)/120.0;
        iq[2*n]=(s16)lround(2048.0*cos(phase));
        iq[2*n+1]=(s16)lround(2048.0*sin(phase));
    }
    return upload_wave(TONE_BYTES);
}
static int start_playback(void)
{
    if(!loaded_bytes) {xil_printf("TX reject: no validated waveform\r\n"); return XST_FAILURE;}
    CHECK(reset_playback());
    XAxiDma_BdRing *ring=XAxiDma_GetTxRing(&playback);
    XAxiDma_Bd template_bd, *bd;
    const unsigned count=4;
    u32 chunk=loaded_bytes/count;
    if(chunk&63u) {xil_printf("TX reject: length must also be multiple of256\r\n");return XST_FAILURE;}
    /* The 2025.2 driver allocates a dummy cyclic tail BD on every RingCreate.
     * Hardware has been reset above, so the old dummy is no longer in use. */
    free(ring->CyclicBd);
    ring->CyclicBd=NULL;
    XAxiDma_BdRingIntDisable(ring,XAXIDMA_IRQ_ALL_MASK);
    CHECK(XAxiDma_BdRingCreate(ring,(UINTPTR)descriptors,(UINTPTR)descriptors,
                              XAXIDMA_BD_MINIMUM_ALIGNMENT,count));
    if(!ring->CyclicBd) return XST_FAILURE;
    XAxiDma_BdClear(&template_bd);
    CHECK(XAxiDma_BdRingClone(ring,&template_bd));
    CHECK(XAxiDma_BdRingAlloc(ring,count,&bd));
    XAxiDma_Bd *first=bd;
    for(unsigned k=0;k<count;k++) {
        CHECK(XAxiDma_BdSetBufAddr(bd,PL_WAVE+(UINTPTR)k*chunk));
        CHECK(XAxiDma_BdSetLength(bd,chunk,ring->MaxTransferLen));
        /* Each descriptor is one complete AXIS packet; the guard ignores
         * TLAST for DAC sample timing but counts packet boundaries in ILA. */
        XAxiDma_BdSetCtrl(bd,XAXIDMA_BD_CTRL_TXSOF_MASK|XAXIDMA_BD_CTRL_TXEOF_MASK);
        XAxiDma_BdSetId(bd,k);
        bd=(XAxiDma_Bd *)XAxiDma_BdRingNext(ring,bd);
    }
    XAxiDma_BdRingEnableCyclicDMA(ring);
    CHECK(XAxiDma_SelectCyclicMode(&playback,XAXIDMA_DMA_TO_DEVICE,1));
    Xil_DCacheFlushRange((UINTPTR)descriptors,sizeof(descriptors));
    CHECK(XAxiDma_BdRingToHw(ring,count,first));
    CHECK(XAxiDma_BdRingStart(ring));
    /* DMA pre-fills while stopped. Guard additionally waits for >=16KiB. */
    usleep(1000);
    Xil_Out32(CONTROL_BASE,1);
    usleep(10000);
    u32 status=tx_status();
    if((status&0xf7u)!=0x97u) {
        xil_printf("TX FAIL status=0x%08x; expected reset/cal/clock/playing and no underrun/bad keep\r\n",status);
        Xil_Out32(CONTROL_BASE,0);
        return XST_FAILURE;
    }
    xil_printf("TX PLAYING status=0x%08x bytes=%d; inspect FSVA and ILA\r\n",status,loaded_bytes);
    return XST_SUCCESS;
}
static void print_status(void)
{
    xil_printf("TX status=0x%08x MM2S_DMASR=0x%08x loaded=%d\r\n",
        tx_status(),XAxiDma_ReadReg(PLAY_BASE,XAXIDMA_SR_OFFSET),loaded_bytes);
}
int vector_tx_init(XRFdc *rf)
{
    xil_printf("TX init: default stopped; no automatic waveform output\r\n");
    Xil_Out32(CONTROL_BASE,0);
    /* Xil_SetTlbAttributes affects only one 2MiB block below 4GiB. Map ALL
     * 128MiB of reserved staging/readback; not just the first 2MiB. */
    for(u32 offset=0;offset<MAX_BYTES;offset+=TX_MMU_BLOCK_BYTES) {
        Xil_SetTlbAttributes(SOURCE+offset,NORM_NONCACHE);
        Xil_SetTlbAttributes(READBACK+offset,NORM_NONCACHE);
    }
    Xil_SetTlbAttributes(MAILBOX,NORM_NONCACHE);
    Xil_Out32(MAILBOX,0);
    Xil_Out32(MAILBOX+4,0); Xil_Out32(MAILBOX+8,0); Xil_Out32(MAILBOX+12,0);
    Xil_Out32(MAILBOX+16,TX_PROTOCOL_VERSION);
    Xil_Out32(MAILBOX+20,MAX_BYTES);
    Xil_Out32(MAILBOX+24,(u32)SOURCE); Xil_Out32(MAILBOX+28,0);
    Xil_Out32(MAILBOX+32,1200000000u); Xil_Out32(MAILBOX+36,4);
    CHECK(XRFdc_StartUp(rf,XRFDC_DAC_TILE,1));
    CHECK(XRFdc_SetupFIFO(rf,XRFDC_DAC_TILE,1,1));
    u32 interpolation,source,words,zone; XRFdc_IPStatus ip;
    XRFdc_Mixer_Settings mixer;
    XRFdc_PLL_Settings pll;
    CHECK(XRFdc_GetInterpolationFactor(rf,1,0,&interpolation));
    CHECK(XRFdc_GetClockSource(rf,XRFDC_DAC_TILE,1,&source));
    CHECK(XRFdc_GetFabWrVldWords(rf,XRFDC_DAC_TILE,1,0,&words));
    CHECK(XRFdc_GetNyquistZone(rf,XRFDC_DAC_TILE,1,0,&zone));
    CHECK(XRFdc_GetMixerSettings(rf,XRFDC_DAC_TILE,1,0,&mixer));
    CHECK(XRFdc_GetPLLConfig(rf,XRFDC_DAC_TILE,1,&pll));
    CHECK(XRFdc_GetIPStatus(rf,&ip));
    xil_printf("DAC Tile1 state=%d; interpolation=%d source=%d words=%d zone=%d mixer=%d/%d NCO=%d kHz\r\n",
        ip.DACTileStatus[1].TileState,interpolation,source,words,zone,
        mixer.MixerType,mixer.MixerMode,(int)(mixer.Freq*1000.0));
    if(interpolation!=2 || source!=XRFDC_EXTERNAL_CLK || words!=8 || zone!=2 ||
       ip.DACTileStatus[1].TileState!=15 || mixer.MixerType!=XRFDC_MIXER_TYPE_FINE ||
       mixer.MixerMode!=XRFDC_MIXER_MODE_C2R || fabs(pll.SampleRate-2.4)>0.000001)
        return XST_FAILURE;
    /* PG269: 1800MHz at Fs2400 in even Nyquist zone folds to +600MHz,
     * i.e. the signed48 register word 0x400000000000. Initial driver shadow
     * is NOT reliable here: this 2025.2 xrfdc.c initializes the DAC shadow
     * Freq from MixerType; generated NCOFreq is also in GHz vs API MHz.
     * Do not patch vendor code or simply loosen the frequency assertion.
     * First verify the actual loaded BIT's tuning word, then reapply the
     * SAME request through the public API while digitally stopped. This
     * seeds the driver's MHz shadow; GetMixerSettings can unfold it again.
     */
    u64 initial_word=dac_nco_word(rf);
    xil_printf("TX DAC NCO register=0x%04x%08x; requested=1800000 kHz, effective=600000 kHz\r\n",
        (u32)(initial_word>>32),(u32)initial_word);
    if(initial_word!=0x400000000000ull) {
        xil_printf("TX FAIL: initial NCO tuning word differs from Fs2400/zone2/1800MHz contract\r\n");
        return XST_FAILURE;
    }
    mixer.Freq=1800.0; /* public RFDC API uses MHz, not GHz */
    mixer.EventSource=XRFDC_EVNT_SRC_IMMEDIATE;
    CHECK(XRFdc_SetMixerSettings(rf,XRFDC_DAC_TILE,1,0,&mixer));
    CHECK(XRFdc_GetMixerSettings(rf,XRFDC_DAC_TILE,1,0,&mixer));
    CHECK(XRFdc_GetFabWrVldWords(rf,XRFDC_DAC_TILE,1,0,&words));
    if(dac_nco_word(rf)!=initial_word || fabs(mixer.Freq-1800.0)>0.001 || words!=8) {
        xil_printf("TX FAIL: canonical NCO reapplication/readback changed hardware contract\r\n");
        return XST_FAILURE;
    }
    xil_printf("TX DAC_NCO_CONTRACT_PASS: request=1800000 kHz, raw=+600000 kHz, words=8; tuning word unchanged\r\n");
    unsigned timeout=10000;
    while((tx_status()&0x87u)!=0x87u && --timeout) usleep(1000);
    if(!timeout) {xil_printf("TX FAIL DDR calibration/DAC clock/reset status=0x%08x\r\n",tx_status());return XST_FAILURE;}
    XAxiCdma_Config *copy_cfg=XAxiCdma_LookupConfig(UPLOAD_BASE);
    XAxiDma_Config *play_cfg=XAxiDma_LookupConfig(PLAY_BASE);
    if(!copy_cfg || !play_cfg) return XST_FAILURE;
    CHECK(XAxiCdma_CfgInitialize(&upload,copy_cfg,copy_cfg->BaseAddress));
    if(upload.MaxTransLen<=0 || (u32)upload.MaxTransLen<TX_COPY_CHUNK_BYTES) {
        xil_printf("TX FAIL: CDMA BTT limit smaller than chunk size\r\n");
        return XST_FAILURE;
    }
    CHECK(XAxiDma_CfgInitialize(&playback,play_cfg));
    if(!XAxiDma_HasSg(&playback)) return XST_FAILURE;
    CHECK(make_tone());
    initialized=1;
    dma_barrier();
    Xil_Out32(MAILBOX,MAILBOX_MAGIC);
    xil_printf("TX READY, stopped. UART: p=play, m=mute/flush, t=load10MHz IQ tone, s=status.\r\n");
    xil_printf("TX PROTOCOL=2 MAX_BYTES=%d source=0x%08x readback=0x%08x; all blocks noncacheable\r\n",
        MAX_BYTES,(u32)SOURCE,(u32)READBACK);
    xil_printf("Host mailbox: 0x20300000 magic; +4 command(1load/2play/3stop/4tone), +8 bytes, +12 result.\r\n");
    return XST_SUCCESS;
}
void vector_tx_poll(void)
{
    if(!initialized) return;
    u32 cmd=Xil_In32(MAILBOX+4);
    int result=XST_SUCCESS;
    if(XUartPs_IsReceiveData(0xFF000000u)) {
        unsigned char key=XUartPs_ReadReg(0xFF000000u,XUARTPS_FIFO_OFFSET);
        if(key=='p') cmd=2; else if(key=='m') cmd=3;
        else if(key=='t') cmd=4; else if(key=='s') print_status();
    }
    if(!cmd) return;
    if(cmd==1) result=upload_wave(Xil_In32(MAILBOX+8));
    else if(cmd==2) result=start_playback();
    else if(cmd==3) result=reset_playback();
    else if(cmd==4) result=make_tone();
    else result=XST_FAILURE;
    Xil_Out32(MAILBOX+12,result==XST_SUCCESS?1u:0xffffffffu);
    dma_barrier();
    Xil_Out32(MAILBOX+4,0);
    print_status();
}
#endif
