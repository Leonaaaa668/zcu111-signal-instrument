set rx_root [file normalize [file join [file dirname [info script]] ..]]
set tx_reports [file join $rx_root reports rx_dma_20261027_01]
file mkdir $tx_reports
set rx_wns [get_property SLACK [get_timing_paths -delay_type max -max_paths 1]]
if {$rx_wns<0 && $rx_wns>=-0.100} {
    report_timing -delay_type max -max_paths 20 -path_type full_clock_expanded -file [file join $tx_reports physical_repair_before_setup.rpt]
    phys_opt_design -directive AggressiveExplore
    route_design -finalize
}
if {[catch {source [file join $rx_root scripts post_route_tx.tcl]} rx_error rx_options]} {
    catch {report_timing -delay_type max -max_paths 30 -path_type full_clock_expanded -file [file join $tx_reports failed_setup.rpt]}
    catch {report_timing -delay_type min -max_paths 20 -path_type full_clock_expanded -file [file join $tx_reports failed_hold.rpt]}
    catch {report_pulse_width -min_skew -limit 100 -file [file join $tx_reports failed_pulse_skew.rpt]}
    catch {write_checkpoint -force [file join $tx_reports failed_routed.dcp]}
    return -options $rx_options $rx_error
}
