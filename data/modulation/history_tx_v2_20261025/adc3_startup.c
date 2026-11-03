/* Standalone A53 application for a NEW platform exported from this design.
 * BSP dependencies: xiicps, rfdc, libmetal, xilstandalone.
 * Reference: Xilinx xrfdc_clk.c (MIT); board SPI commands from its example.
 * Build and physical-test status is recorded separately in reports/.
 */
#include "xparameters.h"
#include "xiicps.h"
#include "xrfdc.h"
#include "xil_printf.h"
#include "sleep.h"
#include "clock_tables.h"
#ifdef ENABLE_VECTOR_TX
#include "vector_tx.h"
#endif

static XIicPs iic;
static XRFdc rf;
static metal_phys_addr_t rf_phys;
static struct metal_device rf_device = {
    .bus = NULL, .num_regions = 1,
    .regions = {{.physmap = &rf_phys, .size = 0x40000,
                 .page_shift = (unsigned)-1, .page_mask = (unsigned)-1,
                 .mem_flags = 0, .ops = {NULL}}},
    .node = {NULL}, .irq_num = 0, .irq_info = NULL
};

static int send_bytes(unsigned addr, unsigned char *data, unsigned count)
{
    int status = XIicPs_MasterSendPolled(&iic, data, count, addr);
    if (status != XST_SUCCESS) return status;
    unsigned timeout = 100000;
    while (XIicPs_BusIsBusy(&iic) && --timeout) usleep(1);
    return timeout ? XST_SUCCESS : XST_FAILURE;
}

static int write_spi(unsigned select, unsigned value, unsigned bytes)
{
    unsigned char tx[5];
    tx[0] = select;
    for (unsigned n = 0; n < bytes; ++n)
        tx[n + 1] = (value >> (8 * (bytes - 1 - n))) & 0xFF;
    int s = send_bytes(0x2F, tx, bytes + 1);
    usleep(25000);
    return s;
}

static int init_clocks(void)
{
    xil_printf("Clock init: lookup PS I2C1\r\n");
#ifdef SDT
    XIicPs_Config *cfg = XIicPs_LookupConfig(0xFF030000u); /* PS I2C1 */
#else
    XIicPs_Config *cfg = XIicPs_LookupConfig(XPAR_XIICPS_1_DEVICE_ID);
#endif
    xil_printf("Clock init: configure I2C controller\r\n");
    if (!cfg || XIicPs_CfgInitialize(&iic, cfg, cfg->BaseAddress) != XST_SUCCESS)
        return XST_FAILURE;
    if (XIicPs_SetSClk(&iic, 100000) != XST_SUCCESS) return XST_FAILURE;
    xil_printf("Clock init: select board I2C mux\r\n");
    unsigned char mux = 0x20; /* I2C1 mux channel 5: analog clock module */
    if (send_bytes(0x74, &mux, 1) != XST_SUCCESS) return XST_FAILURE;
    usleep(25000);
    xil_printf("Clock init: configure SPI bridge\r\n");
    unsigned char mode[2] = {0xF0, 0x02}; /* SC18IS602 SPI configuration */
    if (send_bytes(0x2F, mode, 2) != XST_SUCCESS) return XST_FAILURE;
    usleep(25000);
    xil_printf("Clock init: write LMK registers\r\n");
    for (unsigned n = 0; n < 26; ++n)
        if (write_spi(0x02, adc3_lmk_12288[n], 4) != XST_SUCCESS) return XST_FAILURE;
    sleep(2);
    xil_printf("Clock init: write LMX U102 registers\r\n");
    /* SPI select 0x08 = U102; OUTB supplies bank225/tile1, OUTA bank224.
     * U103 remains unused. U104 is configured separately only in the TX build. */
    if (write_spi(0x08, adc3_lmx_2400[112] | 2u, 3) != XST_SUCCESS) return XST_FAILURE;
    if (write_spi(0x08, adc3_lmx_2400[112] & ~2u, 3) != XST_SUCCESS) return XST_FAILURE;
    for (unsigned n = 0; n < 113; ++n)
        if (write_spi(0x08, adc3_lmx_2400[n], 3) != XST_SUCCESS) return XST_FAILURE;
    usleep(10000);
    if (write_spi(0x08, adc3_lmx_2400[112], 3) != XST_SUCCESS) return XST_FAILURE;
    sleep(2);
    xil_printf("Clock init: register writes complete (PLL lock not independently measured)\r\n");
#ifdef ENABLE_VECTOR_TX
    xil_printf("Clock init: configure DAC LMX U104 (SPI select 0x01)\r\n");
    if (write_spi(0x01, adc3_lmx_2400[112] | 2u, 3) != XST_SUCCESS) return XST_FAILURE;
    if (write_spi(0x01, adc3_lmx_2400[112] & ~2u, 3) != XST_SUCCESS) return XST_FAILURE;
    for (unsigned n=0;n<113;n++)
        if(write_spi(0x01,adc3_lmx_2400[n],3)!=XST_SUCCESS) return XST_FAILURE;
    usleep(10000);
    if(write_spi(0x01,adc3_lmx_2400[112],3)!=XST_SUCCESS) return XST_FAILURE;
    sleep(2);
#endif
    /* These writes are NOT an independent measurement of the external PLL lock. */
    return XST_SUCCESS;
}

int main(void)
{
    xil_printf("ADC3 2400 MSPS startup (physical board validation required)\r\n");
    if (init_clocks() != XST_SUCCESS) {
        xil_printf("FAIL: I2C clock-chip configuration\r\n");
        return XST_FAILURE;
    }
    struct metal_init_params metal_params = METAL_INIT_DEFAULTS;
    xil_printf("RFDC init: libmetal\r\n");
    if (metal_init(&metal_params)) return XST_FAILURE;
#ifdef SDT
    const unsigned rf_id = 0; /* Libmetal region index, NOT the SDT lookup argument. */
    XRFdc_Config *cfg = XRFdc_LookupConfig(0xB0040000u); /* 2025.2 rfdc_v13_0 SDT API */
#else
    const unsigned rf_id = XPAR_XRFDC_0_DEVICE_ID;
    XRFdc_Config *cfg = XRFdc_LookupConfig(rf_id);
#endif
    if (!cfg || cfg->BaseAddr != 0xB0040000u) {
        xil_printf("FAIL: RFDC BSP does not match this hardware\r\n");
        return XST_FAILURE;
    }
    rf_phys = cfg->BaseAddr;
#ifdef SDT
    rf_device.name = cfg->Name;
#else
    rf_device.name = XPAR_XRFDC_0_DEV_NAME;
#endif
    rf_device.regions[0].virt = (void *)cfg->BaseAddr;
    struct metal_device *metal_dev = &rf_device;
    xil_printf("RFDC init: register and initialize driver\r\n");
    if (XRFdc_RegisterMetal(&rf, rf_id, &metal_dev) != XST_SUCCESS ||
        XRFdc_CfgInitialize(&rf, cfg) != XST_SUCCESS) {
        xil_printf("FAIL: RFDC initialization\r\n");
        return XST_FAILURE;
    }
    /* IP-generated configuration already specifies NCO +1800 MHz, real->IQ,
     * decimation2 and zone2. Keep that config; do not use the old TRD ELF. */
    xil_printf("RFDC init: start ADC tile1\r\n");
    if (XRFdc_StartUp(&rf, XRFDC_ADC_TILE, 1) != XST_SUCCESS) {
        xil_printf("FAIL: tile1 startup; check external 2400MHz and PL300MHz\r\n");
        return XST_FAILURE;
    }
    if (XRFdc_SetupFIFO(&rf, XRFDC_ADC_TILE, 1, 1) != XST_SUCCESS) return XST_FAILURE;
    XRFdc_IPStatus status;
    if (XRFdc_GetIPStatus(&rf, &status) != XST_SUCCESS) return XST_FAILURE;
    xil_printf("Tile1 state=%d, enabled=%d\r\n", status.ADCTileStatus[1].TileState,
               status.ADCTileStatus[1].IsEnabled);
    u32 source, decimation, nyquist, words;
    u8 fifo;
    XRFdc_Mixer_Settings mixer;
    /* High-speed dual ADC driver uses physical Block_Id=1, maps to slices2/3. */
    if (XRFdc_GetClockSource(&rf, XRFDC_ADC_TILE, 1, &source) != XST_SUCCESS ||
        XRFdc_GetDecimationFactor(&rf, 1, 1, &decimation) != XST_SUCCESS ||
        XRFdc_GetNyquistZone(&rf, XRFDC_ADC_TILE, 1, 1, &nyquist) != XST_SUCCESS ||
        XRFdc_GetFabRdVldWords(&rf, XRFDC_ADC_TILE, 1, 1, &words) != XST_SUCCESS ||
        XRFdc_GetMixerSettings(&rf, XRFDC_ADC_TILE, 1, 1, &mixer) != XST_SUCCESS ||
        XRFdc_GetFIFOStatus(&rf, XRFDC_ADC_TILE, 1, &fifo) != XST_SUCCESS) {
        xil_printf("FAIL: ADC3 configuration readback\r\n");
        return XST_FAILURE;
    }
    xil_printf("Readback: clk_source=%d decimation=%d nyquist=%d words=%d fifo=%d\r\n",
               source, decimation, nyquist, words, fifo);
    xil_printf("Readback: effective NCO=%d kHz; mixer type=%d mode=%d\r\n",
               (int)(mixer.Freq * 1000.0), mixer.MixerType, mixer.MixerMode);
    if (source != XRFDC_EXTERNAL_CLK || decimation != 2 || nyquist != 2 || words != 4 || !fifo) {
        xil_printf("FAIL: readback differs from the ADC3 hardware contract\r\n");
        return XST_FAILURE;
    }
    xil_printf("Open ILA: probe8/probe9 valid, probe10 locked, probe11 resetn.\r\n");
#ifdef ENABLE_VECTOR_TX
    if(vector_tx_init(&rf)!=XST_SUCCESS) {
        xil_printf("FAIL: vector TX startup; playback remains stopped\r\n");
        return XST_FAILURE;
    }
#endif
    u32 previous_intr = 0;
    for (;;) {
        u32 intr;
        if (XRFdc_GetIntrStatus(&rf, XRFDC_ADC_TILE, 1, 1, &intr) == XST_SUCCESS &&
            intr != previous_intr) {
            xil_printf("ADC3 interrupt/status flags changed: 0x%08x (decode before accepting capture)\r\n", intr);
            previous_intr = intr;
        }
#ifdef ENABLE_VECTOR_TX
        vector_tx_poll();
        usleep(1000);
#else
        sleep(1);
#endif
    }
}
