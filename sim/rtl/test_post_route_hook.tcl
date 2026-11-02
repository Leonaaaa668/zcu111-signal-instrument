# Exercise the ACTUAL hook's parsing/routing-budget/gating logic with mocks.
# This does not run Vivado routing or certify hardware timing.
set test_root [file normalize [file join [file dirname [info script]] ..]]
set test_dir [file join $test_root reports tx hook_selftest [clock seconds]]
file mkdir [file join $test_dir scripts] [file join $test_dir constraints]
file copy [file join $test_root scripts post_route_tx.tcl] [file join $test_dir scripts]
file copy [file join $test_root constraints tx_cdc.xdc] [file join $test_dir constraints]
set test_fh [open [file join $test_root sim fixtures ddr_min_skew.rpt] r]
set test_pulse [read $test_fh]; close $test_fh
proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} {error "HOOK_SELFTEST_FAIL: $message"}
}
proc get_cells {args} {
    if {[string match {*enable_sync*} [lindex $args end]]} {return enable_first}
    return {reset0 reset1 reset2 reset3 reset4 reset5}
}
proc get_pins {args} {
    set pos [lsearch -exact $args -of_objects]
    if {$pos>=0} {return [lindex $args [expr {$pos+1}]]}
    return [lindex $args end]
}
proc set_false_path {args} {}
proc report_pulse_width {args} {
    if {$::test_scenario eq "foreign"} {
        return "Min Skew Slow LIB/D2 LIB/D1 0.090 0.060 -0.030 LOC foreign/path/D2"
    }
    if {$::test_scenario eq "stuck" || ![llength $::test_routes]} {return $::test_pulse}
    return "Min Skew Slow LIB/D2 LIB/D1 0.090 0.140 0.050 LOC adc3_capture_i/ddr4_0/a/xiphy_rxtx_bitslice/D2"
}
proc get_nets {args} {return physical_net}
proc get_net_delays {args} {return interconnect_delay}
proc get_property {property object} {
    switch -- $property {FAST_MIN {return 572.0} SLOW_MAX {return 1217.0}}
    error "Unexpected mock property: $property"
}
proc route_design {args} {
    if {[lindex $args 0] eq "-pins"} {
        set min_pos [lsearch -exact $args -min_delay]
        set max_pos [lsearch -exact $args -max_delay]
        assert {$min_pos>=0 && $max_pos>=0} "both router budgets must be supplied"
        set min_ps [lindex $args [expr {$min_pos+1}]]
        set max_ps [lindex $args [expr {$max_pos+1}]]
        assert {$min_ps>=612 && $min_ps<=622 && $max_ps==1817} "budget units/margins changed"
        lappend ::test_routes $args
    }
}
proc report_timing_summary {args} {
    if {$::test_scenario in {timing stuck}} {return "Timing constraints are not met"}
    return "All user specified timing constraints are met"
}
proc report_route_status {args} {
    if {$::test_scenario eq "routing"} {return "# of nets with routing errors.............. : 1 :"}
    return "# of nets with routing errors.............. : 0 :"
}
set test_hook [file join $test_dir scripts post_route_tx.tcl]
set test_routes {}; set test_scenario repair
source $test_hook
assert {[llength $test_routes]==2} "must repair exactly the two negative real-report endpoints"
foreach test_scenario {foreign timing routing stuck} {
    set test_routes {}
    set result [catch {source $test_hook} message]
    assert {$result==1} "must block $test_scenario failure"
    puts "EXPECTED_REJECTION $test_scenario: $message"
}
puts "TX_POSTROUTE_HOOK_MOCK_PASS; parser and failure gates only, NO Vivado routing or hardware test"
