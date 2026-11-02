`timescale 1ns/1ps
// Verilog wrapper; the bundled configuration crosses using XPM handshake.
module sa_rx_control_bd(
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME ctrl_clk, ASSOCIATED_RESET ctrl_resetn, FREQ_HZ 99999001" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 ctrl_clk CLK" *)
    input wire ctrl_clk,
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME ctrl_resetn, POLARITY ACTIVE_LOW" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 ctrl_resetn RST" *)
    input wire ctrl_resetn,
    input wire [31:0] control,
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME dsp_clk, FREQ_HZ 300000000" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 dsp_clk CLK" *)
    input wire dsp_clk,
    input wire [31:0] dsp_status, output wire [31:0] status,
    output wire [31:0] config_word, output wire config_update
);
    sa_rx_control core(.ctrl_clk(ctrl_clk),.ctrl_resetn(ctrl_resetn),.control(control),
        .dsp_clk(dsp_clk),.dsp_status(dsp_status),.status(status),
        .config_word(config_word),.config_update(config_update));
endmodule
