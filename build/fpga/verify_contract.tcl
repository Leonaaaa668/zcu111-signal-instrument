# Called with the reconstructed BD open; values match the frozen narrow09 design.
foreach {cell key expected} {
    sa_dsp MAX_ZOOM 0 sa_result MAX_LOG_N 16
    sa_fft transform_length 65536 sa_fft run_time_configurable_transform_length true
    rx_dma c_addr_width 40 rx_dma c_include_sg 1 rx_dma c_include_s2mm 1
    rx_dma c_include_mm2s 0 tx_dma c_addr_width 40 tx_dma c_include_sg 1
    rfdc ADC1_Sampling_Rate 2.400 rfdc DAC1_Sampling_Rate 2.400
    rfdc ADC_Data_Width12 4 rfdc ADC_Decimation_Mode12 2
    rfdc DAC_Data_Width10 8 rfdc DAC_Interpolation_Mode10 2
} {
    set actual [get_property CONFIG.$key [get_bd_cells $cell]]
    if {$actual ne $expected} {error "Contract differs: $cell/$key=$actual, expected $expected"}
}
foreach {segment offset} {
    zynq_ultra_ps_e_0/Data/SEG_rx_dma_Reg 0xB00C0000
    zynq_ultra_ps_e_0/Data/SEG_sa_result_reg0 0xB00D0000
} {
    set seg [get_bd_addr_segs -quiet $segment]
    if {[llength $seg]!=1 || [get_property OFFSET $seg]!=$offset} {error "Address contract: $segment"}
}
foreach f [get_files -of_objects [get_filesets sources_1]] {
    if {![file exists $f]} {error "Missing source: $f"}
}
