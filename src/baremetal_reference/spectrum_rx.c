#ifdef ENABLE_RX_DMA
#include "spectrum_rx.h"
#include "rx_memory_map.h"
#include "xaxidma.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xil_mmu.h"
#include "xiltimer.h"
#include "sleep.h"
#include <string.h>
#include <math.h>
#include "sa_packet_validate.h"
/* Mailbox words: magic0,command1,result2,ticket3,epoch4,segment5,NCOseq6,
 * packet_addr7,bytes8,quality9,logN10,zoom11,state12,status13,sequence14/15,
 * version16,maxbytes17,buffer0/1 18/19,counter_epoch20/21,requestedNCO22/23.
 * Host writes only command/ticket/segment/logN/zoom. Commands CAPTURE1,STOP2.
 * result: 0 pending,1 PASS; errors 0x100+line (see UART/source). No TX action.
 */
static XAxiDma rx_dma;
static XRFdc *rx_rf;
static volatile u32 *const mb=(volatile u32 *)(UINTPTR)RX_MAILBOX_ADDR;
static XAxiDma_BdRing *ring;
static unsigned state,buffer_index;
static u32 epoch,ticket,packet_bytes,logn,zoom,control,segment;
static u64 counter_epoch,raw_nco;
static XTime deadline;
static int ready;
static void barrier(void){__asm__ __volatile__("dsb sy":::"memory");}
static u32 read_result(unsigned offset){return Xil_In32(RX_RESULT_BASE+offset);}
static void write_result(unsigned offset,u32 value){Xil_Out32(RX_RESULT_BASE+offset,value);}
static u64 join(u32 low,u32 high){return (u64)low|((u64)high<<32);}
static int expired(void){XTime now;XTime_GetTime(&now);return now>deadline;}
static void timeout_start(void){XTime_GetTime(&deadline);deadline+=3ull*COUNTS_PER_SECOND;}
static u64 read_nco(unsigned slice)
{
    u32 base=XRFDC_BLOCK_BASE(XRFDC_ADC_TILE,1,slice);
    return ((u64)XRFdc_ReadReg16(rx_rf,base,XRFDC_ADC_NCO_FQWD_UPP_OFFSET)<<32)|
      ((u64)XRFdc_ReadReg16(rx_rf,base,XRFDC_ADC_NCO_FQWD_MID_OFFSET)<<16)|
      (u64)XRFdc_ReadReg16(rx_rf,base,XRFDC_ADC_NCO_FQWD_LOW_OFFSET);
}
static int quiesce_result(void)
{
    if(!(read_result(0)&1)){
        /* CANCEL prevents a new private frame; published packets may drain.
         * Vendor DMA reset first completes transactions gracefully. Never
         * asynchronously reset an AXI master while it owns memory requests. */
        write_result(0,2);barrier();usleep(100);
        XAxiDma_Reset(&rx_dma);
        unsigned tries=100000;
        while(!XAxiDma_ResetIsDone(&rx_dma)&&--tries)usleep(1);
        if(!tries)return XST_FAILURE;
    }
    write_result(0,1);barrier();usleep(100);
    return (read_result(0)==1 && !(read_result(4)&0x1f03))?XST_SUCCESS:XST_FAILURE;
}
static int flush_result(void)
{
    if(quiesce_result()!=XST_SUCCESS)return XST_FAILURE;
    /* Only after DMA ownership converges, reset FIFO/snapshot and rebuild BD
     * software bookkeeping. A full hold also drains old CDC handshakes. */
    write_result(0,0);barrier();usleep(100);
    XAxiDma_Reset(&rx_dma);
    unsigned tries=100000;
    while(!XAxiDma_ResetIsDone(&rx_dma)&&--tries)usleep(1);
    if(!tries)return XST_FAILURE;
    for(tries=0;tries<1000;tries++){if(read_result(4)&1)return XST_SUCCESS;usleep(1);}
    return XST_FAILURE;
}
static void finish(u32 result)
{
    if(result!=1){
        xil_printf("RX_DMA_FAIL code=0x%08x ticket=%d; result invalid\r\n",result,ticket);
        if(quiesce_result()!=XST_SUCCESS){
            /* No further automatic commands after a failed graceful halt. */
            ready=0;mb[0]=0;xil_printf("RX_DMA_HALT_FAILED; reinitialize before further use\r\n");
        }
        mb[7]=0;mb[8]=0;
    }
    state=0;mb[12]=0;mb[13]=Xil_In32(RX_GPIO_BASE+8);
    mb[9]=(mb[13]>>4)&1;barrier();mb[2]=result;barrier();mb[1]=0;
}
#define RX_FAIL() do{finish(0x100u+__LINE__);return;}while(0)
static int arm_dma(void)
{
    XAxiDma_Bd template_bd,*bd;
    ring=XAxiDma_GetRxRing(&rx_dma);
    XAxiDma_BdRingIntDisable(ring,XAXIDMA_IRQ_ALL_MASK);
    if(XAxiDma_BdRingCreate(ring,RX_BD_ADDR,RX_BD_ADDR,XAXIDMA_BD_MINIMUM_ALIGNMENT,8)!=XST_SUCCESS)return XST_FAILURE;
    XAxiDma_BdClear(&template_bd);
    if(XAxiDma_BdRingClone(ring,&template_bd)!=XST_SUCCESS ||
       XAxiDma_BdRingAlloc(ring,1,&bd)!=XST_SUCCESS)return XST_FAILURE;
    UINTPTR address=buffer_index?RX_BUFFER1:RX_BUFFER0;
    memset((void *)address,0,RX_MAX_PACKET);
    if(XAxiDma_BdSetBufAddr(bd,address)!=XST_SUCCESS ||
       XAxiDma_BdSetLength(bd,packet_bytes,ring->MaxTransferLen)!=XST_SUCCESS)return XST_FAILURE;
    XAxiDma_BdSetCtrl(bd,0);XAxiDma_BdSetId(bd,address);
    barrier();
    if(XAxiDma_BdRingToHw(ring,1,bd)!=XST_SUCCESS || XAxiDma_BdRingStart(ring)!=XST_SUCCESS)return XST_FAILURE;
    return XST_SUCCESS;
}
static int packet_valid(UINTPTR address)
{
    const struct sa_packet_expect e={epoch,ticket,segment,1,logn,zoom,control,raw_nco,counter_epoch};
    return sa_packet_valid((const void *)address,packet_bytes,&e);
}
int spectrum_rx_init(XRFdc *rf)
{
    rx_rf=rf;ready=0;
    Xil_SetTlbAttributes(RX_MAILBOX_ADDR,NORM_NONCACHE);
    Xil_SetTlbAttributes(RX_BD_ADDR,NORM_NONCACHE);
    barrier();memset((void *)mb,0,128);
    if(Xil_In32(RX_RESULT_BASE+8)!=0x31535852u)return XST_FAILURE;
    XAxiDma_Config *cfg=XAxiDma_LookupConfig(RX_DMA_BASE);
    if(!cfg || cfg->HasMm2S || !cfg->HasS2Mm || !cfg->HasSg || cfg->AddrWidth!=40)return XST_FAILURE;
    /* HOLD_RESET also holds the AXI DMA's axi_resetn low. Its AXI-Lite
     * registers cannot respond until this is released, including accesses
     * performed by CfgInitialize. RX RUN remains zero; no frame is requested. */
    xil_printf("RX DMA init: release hardware reset before driver access\r\n");
    write_result(0,0);barrier();usleep(100);
    if(XAxiDma_CfgInitialize(&rx_dma,cfg)!=XST_SUCCESS || flush_result()!=XST_SUCCESS)return XST_FAILURE;
    /* NCO is read at BOTH internal slices of physical ADC1 (driver uses slice3
     * for high-speed physical Block_Id1). No inferred write of tuning words. */
    raw_nco=read_nco(3);
    if(raw_nco!=0x400000000000ull || read_nco(2)!=raw_nco)return XST_FAILURE;
    if(quiesce_result()!=XST_SUCCESS)return XST_FAILURE;
    mb[16]=1;mb[17]=RX_MAX_PACKET;mb[18]=RX_BUFFER0;mb[19]=RX_BUFFER1;
    mb[22]=1800000000u;mb[23]=0;barrier();mb[0]=RX_MAILBOX_MAGIC;ready=1;
    xil_printf("RX_DMA_READY v1; raw Hann spectrum; VBW bypass; uncalibrated; idle\r\n");
    return XST_SUCCESS;
}
void spectrum_rx_poll(void)
{
    if(!ready)return;
    u32 command=mb[1];
    if(command)barrier(); /* acquire host-owned parameters after command */
    if(command==2){
        if(quiesce_result()!=XST_SUCCESS)RX_FAIL();
        u32 old=Xil_In32(RX_GPIO_BASE);control=(old^0x80000000u)&0x80071f00u;
        Xil_Out32(RX_GPIO_BASE,control);barrier();usleep(100);
        if(!(read_result(0)&1) || (read_result(4)&1))RX_FAIL();
        u32 stopped=Xil_In32(RX_GPIO_BASE+8);
        if((stopped&0x80000001u)!=(control&0x80000000u))RX_FAIL();
        mb[7]=0;mb[8]=0;finish(1);return;
    }
    if(!state && command){
        if(command!=1)RX_FAIL();
        ticket=mb[3];segment=mb[5];logn=mb[10];zoom=mb[11];mb[2]=0;mb[7]=0;mb[8]=0;
        if(!ticket || logn<7 || logn>16 || zoom>6)RX_FAIL();
        if(read_nco(3)!=raw_nco || read_nco(2)!=raw_nco)RX_FAIL();
        if(flush_result()!=XST_SUCCESS)RX_FAIL();
        epoch++;XTime_GetTime(&counter_epoch);control=((Xil_In32(RX_GPIO_BASE)^0x80000000u)&0x80000000u)|(zoom<<16)|(logn<<8)|1;
        Xil_Out32(RX_GPIO_BASE,control);state=1;mb[12]=1;timeout_start();return;
    }
    if(!state)return;
    if(expired())RX_FAIL();
    if(state==1){
        u32 s=Xil_In32(RX_GPIO_BASE+8);
        if((s&0x801fffffu)!=((control&0x80000000u)|(zoom<<18)|(logn<<13)|15u))return;
        packet_bytes=256+(8u<<logn);
        if(arm_dma()!=XST_SUCCESS)RX_FAIL();
        u32 meta[16]={epoch,segment,1,ticket,1800000000u,0,(u32)raw_nco,(u32)(raw_nco>>32),
          (u32)counter_epoch,(u32)(counter_epoch>>32),2400000000u,1200000000u,1,control,0,0x52585331u};
        for(unsigned k=0;k<16;k++)write_result(0x100+4*k,meta[k]);
        barrier();write_result(0,256);barrier();state=2;mb[12]=2;return;
    }
    u32 response=read_result(4),dma_status=XAxiDma_ReadReg(rx_dma.RegBase,XAXIDMA_RX_OFFSET+XAXIDMA_SR_OFFSET);
    if((dma_status&XAXIDMA_ERR_ALL_MASK) || (Xil_In32(RX_GPIO_BASE+8)&16))RX_FAIL();
    if(!(response&4))return;
    if(read_result(12)!=1 || read_result(16)!=ticket || read_result(32)!=epoch)RX_FAIL();
    XAxiDma_Bd *done;int count=XAxiDma_BdRingFromHw(ring,1,&done);
    if(!count)return;
    u32 bd_status=XAxiDma_BdGetSts(done),actual=XAxiDma_BdGetActualLength(done,ring->MaxTransferLen);
    if(count!=1 || (bd_status&XAXIDMA_BD_STS_ALL_ERR_MASK) ||
       (bd_status&(XAXIDMA_BD_STS_RXSOF_MASK|XAXIDMA_BD_STS_RXEOF_MASK))!=(XAXIDMA_BD_STS_RXSOF_MASK|XAXIDMA_BD_STS_RXEOF_MASK) || actual!=packet_bytes)RX_FAIL();
    barrier();UINTPTR address=buffer_index?RX_BUFFER1:RX_BUFFER0;
    if(read_nco(3)!=raw_nco || read_nco(2)!=raw_nco)RX_FAIL();
    if(!packet_valid(address))RX_FAIL();
    const u32 *h=(const u32 *)address;
    if(join(h[5],h[6])!=join(read_result(24),read_result(28)))RX_FAIL();
    if(XAxiDma_BdRingFree(ring,1,done)!=XST_SUCCESS)RX_FAIL();
    mb[3]=ticket;mb[4]=epoch;mb[6]=1;mb[7]=(u32)address;mb[8]=actual;
    mb[14]=h[5];mb[15]=h[6];mb[20]=(u32)counter_epoch;mb[21]=(u32)(counter_epoch>>32);
    buffer_index^=1;
    xil_printf("RX_DMA_PACKET_PASS ticket=%d epoch=%d bytes=%d; raw spectrum ready\r\n",ticket,epoch,actual);
    finish(1);
}
#endif
