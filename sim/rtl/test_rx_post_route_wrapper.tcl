# Offline branch tests; all Vivado commands mocked, no hardware APIs.
set root [file normalize [file join [file dirname [info script]] ..]]
set fixture [file join $root reports rx_debug_20261026_01 wrapper_mock_[clock microseconds]]
file mkdir [file join $fixture scripts]
file copy [file join $root scripts post_route_rx_debug.tcl] [file join $fixture scripts]
rename source native_source
proc source {path} {
    if {[file tail $path] eq "post_route_tx.tcl"} {
        incr ::gates
        if {$::slack<0} {error NATIVE_TIMING_REJECTED}
        return STRICT_GATE_MOCK_PASS
    }
    uplevel 1 [list native_source $path]
}
proc get_timing_paths {args} {return path0}
proc get_property {property object} {
    if {$property ne "SLACK" || $object ne "path0"} {error "Unexpected property"}
    return $::slack
}
proc report_timing {args} {incr ::diagnostics}
proc report_pulse_width {args} {incr ::diagnostics}
proc phys_opt_design {args} {
    if {$args ne {-directive AggressiveExplore}} {error "Unexpected optimization directive"}
    incr ::optimizations
    set ::slack $::after_slack
}
proc route_design {args} {
    if {$args ne {-finalize}} {error "Unexpected route operation"}
    incr ::finalizations
}
proc write_checkpoint {args} {incr ::checkpoints}
foreach {scenario before after expected_opt expected_error} {
    already_pass 0.010 0.010 0 0
    small_fixed -0.010 0.025 1 0
    small_unfixed -0.010 -0.005 1 1
    large_rejected -0.200 -0.200 0 1
} {
    set slack $before;set after_slack $after
    set optimizations 0;set finalizations 0;set diagnostics 0;set gates 0;set checkpoints 0
    set code [catch {source [file join $fixture scripts post_route_rx_debug.tcl]} result]
    if {$code!=$expected_error || $optimizations!=$expected_opt || $finalizations!=$expected_opt || $gates!=1 || $checkpoints!=$expected_error} {
        error "Wrapper branch failed: $scenario $code $result"
    }
    if {$expected_error && $result ne "NATIVE_TIMING_REJECTED"} {error "Original signoff failure was lost"}
    puts "RX_WRAPPER_MOCK_CASE_PASS $scenario"
}
puts "RX_POSTROUTE_WRAPPER_MOCK_PASS; no physical optimization, routing or hardware was performed"
