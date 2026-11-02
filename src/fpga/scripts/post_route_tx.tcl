# Same-project ROUTE_DESIGN.TCL.POST hook. No board access or logic changes.
set tx_root [file normalize [file join [file dirname [info script]] ..]]
if {![info exists tx_reports]} {set tx_reports [file join $tx_root reports tx]}
file mkdir $tx_reports
source [file join $tx_root constraints tx_cdc.xdc]
if {[llength $tx_enable_stage0]!=1 || [llength $tx_async_clear]!=6} {
    error "TX CDC endpoint contract changed; refusing empty/broad exceptions"
}
for {set tx_pass 0} {$tx_pass<3} {incr tx_pass} {
    set tx_pw [report_pulse_width -min_skew -limit 100 -return_string]
    set tx_bad [dict create]
    foreach tx_line [split $tx_pw \n] {
        set tx_line [string trim $tx_line]
        if {[regexp {^Min Skew\s+\S+\s+\S+\s+\S+\s+(\S+)\s+(\S+)\s+(\S+)\s+\S+\s+(\S+)\s*$} $tx_line -> tx_req tx_actual tx_slack tx_pin] && $tx_slack<0} {
            if {![string match {adc3_capture_i/ddr4_0/*/xiphy_rxtx_bitslice/D*} $tx_pin]} {
                error "Unexpected min-skew endpoint: $tx_pin"
            }
            if {![dict exists $tx_bad $tx_pin] || [dict get $tx_bad $tx_pin]<$tx_req-$tx_actual} {
                dict set tx_bad $tx_pin [expr {$tx_req-$tx_actual}]
            }
        }
    }
    if {![dict size $tx_bad]} {break}
    if {[dict size $tx_bad]>16} {error "Too many DDR min-skew endpoints for a bounded local repair"}
    dict for {tx_name tx_deficit} $tx_bad {
        set tx_pin [get_pins $tx_name]
        set tx_net [get_nets -top_net_of_hierarchical_group -segments -of_objects $tx_pin]
        if {[llength $tx_net]!=1} {error "DDR physical net ambiguous: $tx_name"}
        set tx_delay [get_net_delays -of_objects $tx_net -to $tx_pin -interconnect_only]
        if {[llength $tx_delay]!=1} {error "DDR interconnect delay missing: $tx_name"}
        # Router guidance is in ps, NOT ns. Both bounds must be supplied.
        # These are routing budgets, not replacements for native timing checks.
        set tx_min [expr {int(ceil([get_property FAST_MIN $tx_delay]+max(40.0,$tx_deficit*1000.0+20.0)))}]
        set tx_max [expr {int(ceil([get_property SLOW_MAX $tx_delay]+600.0))}]
        puts "TX_LOCAL_SKEW_REPAIR pass=$tx_pass pin=$tx_name deficit_ns=$tx_deficit min_ps=$tx_min max_ps=$tx_max"
        route_design -unroute -pins $tx_pin
        route_design -pins $tx_pin -min_delay $tx_min -max_delay $tx_max
    }
    route_design -finalize
}
set tx_timing [report_timing_summary -max_paths 5 -return_string]
set tx_fh [open [file join $tx_reports postroute_signoff.rpt] w]
puts $tx_fh $tx_timing; close $tx_fh
if {![string match {*All user specified timing constraints are met*} $tx_timing]} {
    error "TX post-route signoff failed; bitstream generation is blocked"
}
set tx_route [report_route_status -return_string]
set tx_fh [open [file join $tx_reports postroute_route_status.rpt] w]
puts $tx_fh $tx_route; close $tx_fh
if {![regexp {# of nets with routing errors[. ]*:\s+0\s+:} $tx_route]} {
    error "TX local route has incomplete/conflicting routing; bitstream generation is blocked"
}
puts "TX_POSTROUTE_ALL_TIMING_AND_ROUTE_PASS; native setup/hold/pulse/skew checks retained"
