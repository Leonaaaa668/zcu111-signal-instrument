# OFFLINE MOCK. Remove every actual hardware primitive BEFORE source execution.
set root [file normalize [file join [file dirname [info script]] ..]]
foreach cmd {connect disconnect jtag targets mrd mwr loadhw after} {
    if {[llength [info commands ::$cmd]]} {rename ::$cmd ::mock_original_$cmd}
}
rename ::file ::mock_original_file
proc file {action args} {
    if {$action eq "isfile" && [string match */board/rx_dma_20261027_01/adc_dac_rx_dma.xsa [lindex $args 0]]} {return 1}
    return [uplevel 1 [linsert $args 0 ::mock_original_file $action]]
}
set tag host_mock_01
if {$argc==1} {set tag [lindex $argv 0]}
set out [file join $root reports rx_dma_20261027_01 $tag]
if {[file exists $out]} {error "Use fresh mock fixture directory"}
file mkdir $out
set fh [open [file join $out SYNTHETIC_ONLY.txt] w];puts $fh "No hardware access. Mailbox/DDR fault-injection fixtures.";close $fh
set trace {};set fault none;array set mb {};set gpio 0;set ctl 1;set stat 0;set cases 0
proc reset_mock {} {
    array unset ::mb;for {set k 0} {$k<32} {incr k} {set ::mb($k) 0}
    foreach {k v} {0 0x52584431 4 23 16 1 17 524544 18 0x30010000 19 0x30100000 22 1800000000 23 0} {set ::mb($k) [expr {$v+0}]}
    set ::trace {};set ::fault none;set ::gpio 0;set ::ctl 1;set ::stat 0
}
proc connect {args} {lappend ::trace connect_mock}
proc disconnect {args} {lappend ::trace disconnect_mock}
proc jtag {args} {return "Mock xczu28dr"}
proc targets {args} {return "Mock PSU"}
proc after {args} {}
proc loadhw {args} {
    if {[lindex $args end] ne {{0x20400000 0x2040007f} {0x30000000 0x301fffff} {0xB00B0000 0xB00DFFFF}}} {error "Memory windows broadened"}
    lappend ::trace loadhw_mock
}
proc putword {raw offset value} {return [string replace $raw $offset [expr {$offset+3}] [binary format i $value]]}
proc mrd {args} {
    if {[lindex $args 0] eq "-bin"} {
        if {[lindex $args 1] ne "-file" || [lindex $args 3]!=0x30010000 || [lindex $args 4]!=2112} {error "Unexpected binary download"}
        set f [open [file join $::root reports rx_dma_20261027_01 c_packet_repro_08 packet_1.bin] rb];set b [read $f];close $f
        foreach {word value} [list 5 1 6 0 7 $::mb(4) 8 0 10 $::mb(3) 30 12345 31 0] {set b [putword $b [expr {4*$word}] $value]}
        if {$::fault eq "short"} {set b [string range $b 0 end-4]}
        set f [open [lindex $args 2] wb];puts -nonewline $f $b;close $f;return
    }
    if {[lindex $args 0] ne "-value"} {error "Unexpected mrd mode"}
    set address [expr {[lindex $args end]+0}]
    if {$address>=0x20400000 && $address<0x20400080} {return $::mb([expr {($address-0x20400000)/4}])}
    switch $address {
        2953510920 {return $::gpio}
        2953641984 {return $::ctl}
        2953641988 {return $::stat}
        2953641992 {return 0x31535852}
        default {error [format "Unexpected register read 0x%X" $address]}
    }
}
proc mwr {address value} {
    set address [expr {$address+0}]
    if {$address<0x20400000 || $address>=0x20400080 || ($address&3)} {error "Write outside RX mailbox"}
    set index [expr {($address-0x20400000)/4}]
    if {$index ni {1 2 3 5 10 11}} {error "Write to firmware-owned mailbox"}
    set ::mb($index) $value;lappend ::trace [list write $index $value]
    if {$index==1 && $value==1} {
        if {$::fault eq "timeout"} {return}
        set ::mb(1) 0;set ::mb(2) 1;incr ::mb(4);set ::mb(6) 1;set ::mb(7) 0x30010000;set ::mb(8) 8448;set ::mb(14) 1;set ::mb(20) 12345
        if {$::fault eq "ticket"} {incr ::mb(3)}
        if {$::fault eq "quality"} {set ::mb(9) 1}
        if {$::fault eq "address"} {set ::mb(7) 0x24000000}
        if {$::fault eq "bytes"} {set ::mb(8) 8440}
        if {$::fault eq "epoch"} {incr ::mb(4) -1}
        if {$::fault eq "firmware"} {set ::mb(2) 0x123}
        set ::gpio 15;set ::ctl 0;set ::stat 1
    } elseif {$index==1 && $value==2} {
        set ::mb(1) 0;set ::mb(2) 1;set ::mb(7) 0;set ::mb(8) 0;set ::gpio 0;set ::ctl 1;set ::stat 0
        if {$::fault eq "stop"} {set ::stat 0x100}
    }
}
proc action {args} {
    set ::argv $args;set ::argc [llength $args]
    uplevel #0 [list source [file join $::root scripts rx_dma_command.tcl]]
    incr ::cases
}
reset_mock;action status
foreach item $trace {if {[lindex $item 0] eq "write"} {error "Read-only status changed hardware"}}
reset_mock;action capture [file join $out good.bin] 10 0
if {$ctl!=1 || $stat || $mb(1) || $mb(7) || $gpio} {error "Successful capture did not stop"}
foreach f {ticket quality address bytes epoch firmware timeout short stop} {
    reset_mock;set fault $f
    if {![catch {action capture [file join $out $f.bin] 10 0} err]} {error "Fault $f was accepted"}
    if {[lindex $trace end] ne "disconnect_mock"} {error "Connection leaked on $f"}
    incr cases
}
reset_mock;set mb(0) 0
if {![catch {action stop}]} {error "Old firmware accepted"}
foreach item $trace {if {[lindex $item 0] eq "write"} {error "Old firmware received a command"}}
incr cases
reset_mock
if {![catch {action capture [file join $out invalid.bin] 6 0}] || [llength $trace]} {error "Invalid arguments reached hardware"}
incr cases
foreach config {{7 0} {16 0} {10 1}} {
    reset_mock
    if {![catch {action capture [file join $out outside_release.bin] {*}$config}] || [llength $trace]} {error "Unreleased FFT/zoom reached hardware"}
    incr cases
}
puts "RX_DMA_COMMAND_MOCK_PASS cases=$cases; hardware primitives unavailable"
