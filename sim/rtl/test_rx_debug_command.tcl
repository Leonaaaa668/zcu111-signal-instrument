# MOCK ONLY: replace every hardware command before evaluating control script.
set root [file normalize [file join [file dirname [info script]] ..]]
foreach cmd {connect disconnect jtag targets mrd mwr loadhw after} {
    if {[llength [info commands ::$cmd]]} {rename ::$cmd ::mock_original_$cmd}
}
rename ::file ::mock_original_file
proc file {action args} {
    if {$action eq "exists" && [string match */board/rx_debug_20261026_01/adc_dac_rx_debug.xsa [lindex $args 0]]} {return 1}
    return [uplevel 1 [linsert $args 0 ::mock_original_file $action]]
}
set control 0x1000;set bad_quality 0;set stale 0;set trace {}
proc connect {args} {lappend ::trace connect_mock}
proc disconnect {args} {lappend ::trace disconnect_mock}
proc jtag {args} {return "Mock xczu28dr_0"}
proc targets {args} {return "Mock PSU"}
proc after {args} {}
proc loadhw {args} {
    if {[lindex $args end] ne {{0xB00B0000 0xB00BFFFF}}} {error "RX memory window broadened"}
    lappend ::trace loadhw_mock
}
proc mrd {args} {
    set addr [lindex $args end]
    if {$addr eq "0xB00B0000"} {return [expr {$::control}]}
    if {$addr ne "0xB00B0008"} {error "Unexpected mock read $addr"}
    if {$::stale} {return 0}
    return [expr {($::control&0x80000000)|((($::control>>16)&7)<<18)|((($::control>>8)&31)<<13)|
        (($::control&1) ? 15:8)|($::bad_quality ? 16:0)}]
}
proc mwr {addr value} {
    if {$addr ne "0xB00B0000"} {error "Write outside RX GPIO"}
    set ::control $value;lappend ::trace [list write $addr $value]
}
proc test_action {args} {
    set ::argv $args;set ::argc [llength $args];set ::trace {}
    uplevel #0 [list source [file join $::root scripts rx_debug_command.tcl]]
}
test_action status
foreach item $trace {if {[lindex $item 0] eq "write"} {error "Status wrote hardware"}}
test_action start
if {$control!=0x80000a01} {error "Default FFT1024 start or commit incorrect"}
test_action start 7 2
if {$control!=0x00020701} {error "Reconfigure encoding incorrect"}
test_action stop
if {$control&3} {error "RX stop left RUN/reset request active"}
if {![catch {test_action start 6 0}] || [llength $trace]} {error "Invalid logN reached hardware"}
if {![catch {test_action start 10 7}] || [llength $trace]} {error "Invalid zoom reached hardware"}
set stale 1
if {![catch {test_action start}] || [lindex $trace end] ne "disconnect_mock"} {error "Stale configuration not rejected/cleaned up"}
set stale 0;set bad_quality 1
if {![catch {test_action start}] || [lindex $trace end] ne "disconnect_mock"} {error "Bad quality accepted or connection leaked"}
puts "RX_DEBUG_COMMAND_MOCK_PASS; no hardware commands were available"
