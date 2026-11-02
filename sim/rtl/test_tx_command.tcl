# Mock-only test: NO hardware connection is possible through these commands.
set root [file normalize [file join [file dirname [info script]] ..]]
foreach cmd {connect disconnect jtag targets mrd mwr dow loadhw} {
    if {[llength [info commands ::$cmd]]} {rename ::$cmd ::test_original_$cmd}
}
set test_magic 0x54585757
set test_version 2
set test_command 0
set test_result 1
set test_trace {}
proc connect {args} {lappend ::test_trace connect_mock}
proc disconnect {args} {lappend ::test_trace disconnect_mock}
proc jtag {args} {return "Mock xczu28dr_0"}
proc targets {args} {return "Mock PSU"}
proc loadhw {args} {
    if {[lindex $args end] ne {{0xB00A0000 0xB00AFFFF}}} {error "Unexpected status memory map range"}
    lappend ::test_trace hardware_metadata_only_mock
}
proc mrd {args} {
    set addr [lindex $args end]
    switch -- $addr {
        0x20300000 {return [expr {$::test_magic}]}
        0x20300004 {return $::test_command}
        0x2030000c {return $::test_result}
        0x20300010 {return $::test_version}
        0x20300014 {return 67108864}
        0x20300018 {return 603979776}
        0x2030001c {return 0}
        0x20300020 {return 1200000000}
        0x20300024 {return 4}
        0xB00A0008 {return 135}
        default {error "Unexpected mock read $addr"}
    }
}
proc mwr {addr value} {
    lappend ::test_trace [list write $addr $value]
    if {$addr eq "0x20300004"} {
        lappend ::test_trace [list command $value]
        set ::test_command 0
        set ::test_result 1
    }
}
proc dow {args} {lappend ::test_trace [linsert $args 0 download_mock]}
proc test_action {action args} {
    set ::argv [linsert $args 0 $action]
    set ::argc [llength $::argv]
    set ::test_trace {}
    uplevel #0 [list source [file join $::root scripts tx_command.tcl]]
}
test_action load [file join $root data tx tone_p10mhz.bin]
set commands {}
foreach entry $test_trace {if {[lindex $entry 0] eq "command"} {lappend commands [lindex $entry 1]}}
if {$commands ne {3 1}} {error "Load must stop then upload, NEVER play: $commands"}
test_action play
if {[lsearch -exact $test_trace {command 2}] < 0} {error "Play command missing"}
test_action stop
if {[lsearch -exact $test_trace {command 3}] < 0} {error "Stop command missing"}
test_action status
test_action info
# Sparse fixtures: exercise REAL file-size validation without a real download.
set fixture_dir [file join $root reports tx host_mock_fixtures]
file mkdir $fixture_dir
foreach bytes {48000000 67108864 67109120 257} {
    set path [file join $fixture_dir ${bytes}.bin]
    set fh [open $path wb]; seek $fh [expr {$bytes-1}]; puts -nonewline $fh \0; close $fh
    if {$bytes<=67108864 && $bytes%256==0} {
        test_action load $path
        if {[lsearch -exact $test_trace [list download_mock -data [file normalize $path] 0x24000000]]<0} {
            error "Long upload not directed to reserved v2 source"
        }
        if {[lsearch -exact $test_trace {command 2}]>=0} {error "Unexpected autoplay"}
    } else {
        if {![catch {test_action load $path} message]} {error "Invalid length accepted: $bytes"}
        foreach entry $test_trace {if {[lindex $entry 0] in {write download_mock}} {error "Invalid size caused write"}}
    }
}
set test_version 1
if {![catch {test_action play} message] || ![string match {*refusing writes*} $message]} {error "Missing capability gate"}
set test_version 2
set test_magic 0x54585756
if {![catch {test_action load $path} message]} {error "Old v1 firmware accepted"}
foreach entry $test_trace {if {[lindex $entry 0] in {write download_mock}} {error "Old firmware caused write"}}
set test_magic 0
if {![catch {test_action play} message] || ![string match {*not READY*} $message]} {error "Missing startup gate"}
puts "TX_HOST_COMMAND_MOCK_PASS: 48MB/64MiB accepted; oversized/misaligned/v1 rejected before writes; no autoplay. NO HARDWARE ACCESSED"
