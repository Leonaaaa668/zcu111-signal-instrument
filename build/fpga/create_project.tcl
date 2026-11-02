# Vivado 2025.2. Source-only reconstruction; no synthesis or hardware access.
set package_root [file normalize [file join [file dirname [info script]] ../..]]
set fpga_root [file join $package_root src fpga]
set out [file join $package_root generated vivado]
if {$argc>0} {set out [file normalize [lindex $argv 0]]}
if {[version -short] ne "2025.2"} {error "Use Vivado 2025.2 for the frozen IP versions"}
if {[file exists [file join $out zcu111_v16.xpr]]} {error "Project exists; choose a new output directory"}
create_project zcu111_v16 $out -part xczu28dr-ffvg1517-2-e
set_property board_part xilinx.com:zcu111:part0:1.4 [current_project]
set_property target_language Verilog [current_project]
set_property simulator_language Mixed [current_project]
set_param general.maxThreads 4
add_files -norecurse [glob -directory [file join $fpga_root rtl] *.v *.vh]
set_property include_dirs [list [file join $fpga_root rtl]] [get_filesets sources_1]
set mem [glob -directory [file join $fpga_root coeff sa] *.mem]
add_files -norecurse $mem
foreach f $mem {set_property file_type {Memory Initialization Files} [get_files $f]}
add_files -fileset constrs_1 -norecurse [file join $fpga_root constraints ddr_cons.xdc]
set cdc [file join $fpga_root constraints tx_cdc.xdc]
add_files -fileset constrs_1 -norecurse $cdc
set_property used_in_synthesis false [get_files $cdc]
set_property processing_order LATE [get_files $cdc]
source [file join $fpga_root bd adc3_capture.tcl]
if {[current_bd_design -quiet] ne "adc3_capture"} {error "BD reconstruction did not complete"}
validate_bd_design -force
save_bd_design
set bd [get_files */adc3_capture.bd]
set wrapper [make_wrapper -files $bd -top]
add_files -norecurse $wrapper
set_property top adc3_capture_wrapper [get_filesets sources_1]
set_property STEPS.ROUTE_DESIGN.TCL.POST [file join $fpga_root scripts post_route_rx_dma.tcl] [get_runs impl_1]
update_compile_order -fileset sources_1
source [file join $package_root build fpga verify_contract.tcl]
write_bd_tcl -force [file join $out restored_bd.tcl]
report_ip_status -file [file join $out ip_status.rpt]
puts "V16_SOURCE_PROJECT_RESTORED_CONTRACT_PASS; no synthesis/implementation/board actions"
close_project
