# Explicit full build entry. No JTAG, download, reset or RF control.
if {$argc!=2} {error "Usage: -tclargs project.xpr absolute_export_directory"}
set xpr [file normalize [lindex $argv 0]]
set exports [file normalize [lindex $argv 1]]
file mkdir $exports
open_project $xpr
open_bd_design [get_files */adc3_capture.bd]
source [file join [file dirname [info script]] verify_contract.tcl]
generate_target all [get_files */adc3_capture.bd]
launch_runs synth_1 -jobs 4
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {error "Synthesis failed"}
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {error "Implementation/bitstream failed"}
open_run impl_1
set timing [report_timing_summary -max_paths 10 -return_string]
if {![string match {*All user specified timing constraints are met*} $timing]} {error "Native timing check failed"}
report_timing_summary -max_paths 10 -file [file join $exports timing_summary.rpt]
report_utilization -file [file join $exports utilization.rpt]
report_route_status -file [file join $exports route_status.rpt]
report_drc -file [file join $exports drc.rpt]
report_cdc -details -file [file join $exports cdc.rpt]
write_debug_probes -force [file join $exports adc3_capture.ltx]
file copy -force [file join [get_property DIRECTORY [get_runs impl_1]] adc3_capture_wrapper.bit] [file join $exports fpga.bit]
write_hw_platform -fixed -include_bit -force -file [file join $exports system.xsa]
puts "V16_FULL_BUILD_EXPORTED; inspect new reports; no board actions"
close_project
