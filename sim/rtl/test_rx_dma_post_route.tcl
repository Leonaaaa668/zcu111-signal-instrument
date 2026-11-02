# Pure mock of the new wrapper: native signoff failures must propagate.
set root [file normalize [file join [file dirname [info script]] ..]]
rename source real_source
set initial_wns 0.05;set strict_fail 0;set strict_calls 0;set physical 0;set routes 0;set diagnostics 0
proc source {path} {
    if {[file tail $path] eq "post_route_tx.tcl"} {
        incr ::strict_calls
        if {$::strict_fail} {error "MOCK_NATIVE_SIGNOFF_FAILED"}
        return
    }
    uplevel 1 [list real_source $path]
}
proc get_timing_paths {args} {return mock_path}
proc get_property {property object} {if {$property ne "SLACK"} {error "Unexpected property"};return $::initial_wns}
proc report_timing {args} {incr ::diagnostics}
proc report_pulse_width {args} {incr ::diagnostics}
proc write_checkpoint {args} {incr ::diagnostics}
proc phys_opt_design {args} {if {$args ne {-directive AggressiveExplore}} {error "Unbounded physical change"};incr ::physical}
proc route_design {args} {if {$args ne {-finalize}} {error "Unexpected route command"};incr ::routes}
set wrapper [file join $root scripts post_route_rx_dma.tcl]
real_source $wrapper
if {$strict_calls!=1 || $physical || $routes} {error "Positive timing wrapper altered design or skipped signoff"}
set initial_wns -0.01;set strict_calls 0
real_source $wrapper
if {$strict_calls!=1 || $physical!=1 || $routes!=1} {error "Small residual must get one bounded physical pass and native signoff"}
set initial_wns -0.2;set strict_calls 0;set strict_fail 1;set previous_physical $physical
if {![catch {real_source $wrapper} reason] || $reason ne "MOCK_NATIVE_SIGNOFF_FAILED" || $strict_calls!=1 || $physical!=$previous_physical} {error "Native failure swallowed or unexpected extra repair"}
puts "RX_DMA_POST_ROUTE_WRAPPER_MOCK_PASS; no real physical optimization, checkpoint or hardware action"
