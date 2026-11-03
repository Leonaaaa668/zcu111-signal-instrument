# Volatile command/data access to the integrated baremetal TX application.
# Never resets/programs/erases the board. Loading does NOT start playback.
if {$argc < 1} {error "Usage: tx_command.tcl info|status|stop|tone|play|load <file.bin>"}
set action [lindex $argv 0]
if {$action ni {info status stop tone play load}} {error "Unknown TX action"}
connect -url tcp:127.0.0.1:3121
if {![regexp -nocase {zu28dr} [jtag targets]]} {error "Expected ZCU111 ZU28DR"}
targets -set -nocase -filter {name =~ "PSU"}
set mailbox 0x20300000
if {[mrd -value $mailbox] != 0x54585757} {error "Integrated TX v2 app is not READY or old firmware; read UART first"}
if {[mrd -value 0x20300010]!=2 || [mrd -value 0x20300014]!=67108864 ||
    [mrd -value 0x20300018]!=0x24000000 || [mrd -value 0x2030001c]!=0 ||
    [mrd -value 0x20300020]!=1200000000 || [mrd -value 0x20300024]!=4} {
    error "Unexpected TX v2 capability/memory map; refusing writes"
}
proc tx_issue {command} {
    mwr 0x2030000c 0
    mwr 0x20300004 $command
    for {set k 0} {$k < 12000} {incr k} {
        if {[mrd -value 0x20300004] == 0} {
            set result [mrd -value 0x2030000c]
            if {$result != 1} {error "TX command failed; result=$result. Inspect UART/ILA"}
            return
        }
        after 10
    }
    error "TX command timed out; no success assumed"
}
switch $action {
    info {puts "TX_V2_CAPABILITIES max_bytes=67108864 source=0x24000000 FsIQ=1200000000 IQbytes=4 max_duration_us=13981.013333"}
    status {
        # XSCT blocks PL slave reads until the matching hardware memory map
        # is loaded. Load metadata for ONLY this known GPIO range; do not
        # globally force arbitrary memory access or change board registers.
        set tx_root [file normalize [file join [file dirname [info script]] ..]]
        loadhw -hw [file join $tx_root board combined adc_dac_ddr.xsa] -regs -mem-ranges [list [list 0xB00A0000 0xB00AFFFF]]
        puts "TX_GPIO_STATUS [mrd -value 0xB00A0008]"
    }
    stop {tx_issue 3; puts "TX_STOPPED: digital zero stream, not an RF isolation switch"}
    tone {tx_issue 4; puts "TX_TONE_LOADED: +10MHz IQ, 2048 counts, NOT playing"}
    play {tx_issue 2; puts "TX_PLAY_COMMAND_ACCEPTED: verify UART/ILA and instrument"}
    load {
        if {$argc != 2} {error "load requires a binary waveform path"}
        set waveform [file normalize [lindex $argv 1]]
        set bytes [file size $waveform]
        if {$bytes <= 0 || $bytes > 67108864 || ($bytes % 256) != 0} {
            error "Waveform must be <=64MiB and a multiple of256bytes (64 IQ pairs)"
        }
        tx_issue 3
        puts "TX_UPLOAD_BEGIN bytes=$bytes; JTAG upload may take time; no playback starts automatically"
        dow -data $waveform 0x24000000
        mwr 0x20300008 $bytes
        tx_issue 1
        puts "TX_BINARY_LOADED_AND_DMA_ROUNDTRIP_CHECKED: $waveform; NOT playing"
    }
}
disconnect
