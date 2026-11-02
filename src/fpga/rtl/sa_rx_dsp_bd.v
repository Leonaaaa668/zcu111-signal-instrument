`timescale 1ns/1ps
// Verilog module-reference boundary required by Vivado IP Integrator.
module sa_rx_dsp_bd #(
    parameter COEFF0="d2_1200_to600.mem", parameter COEFF1="d2_600_to300.mem",
    parameter COEFF2="d2_300_to150.mem", parameter COEFF_ZOOM="zoom_d2_normalized.mem",
    parameter HANN_FILE="hann_q17.mem", parameter integer PRIME_SAMPLES=1024,
    parameter integer MAX_ZOOM=0
)(
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME clk, ASSOCIATED_RESET rst_n, FREQ_HZ 300000000" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 clk CLK" *)
    input wire clk,
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rst_n, POLARITY ACTIVE_LOW" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 rst_n RST" *)
    input wire rst_n,
    input wire [63:0] adc_i, input wire [63:0] adc_q,
    input wire adc_i_valid, input wire adc_q_valid,
    input wire [31:0] config_word, input wire config_update,
    output wire fft_resetn, output wire [31:0] fft_config_data, output wire fft_config_valid,
    input wire fft_config_ready,
    output wire [47:0] fft_input_data, output wire fft_input_valid, output wire fft_input_last,
    input wire fft_input_ready,
    input wire [47:0] fft_output_data, input wire fft_output_valid, input wire fft_output_last,
    input wire [23:0] fft_output_user, input wire [5:0] fft_events,
    output wire power_valid, output wire [63:0] power_data, output wire power_last,
    output wire [15:0] power_index, output wire [31:0] quality,
    output wire [63:0] frame_count, output wire [63:0] sample_counter, output wire [31:0] status,
    output wire [4:0] actual_log_n, output wire [4:0] fft_right_shift
);
    sa_rx_dsp #(.COEFF0(COEFF0),.COEFF1(COEFF1),.COEFF2(COEFF2),.COEFF_ZOOM(COEFF_ZOOM),
        .HANN_FILE(HANN_FILE),.PRIME_SAMPLES(PRIME_SAMPLES),.MAX_ZOOM(MAX_ZOOM)) core(
        .clk(clk),.rst_n(rst_n),.adc_i(adc_i),.adc_q(adc_q),
        .adc_i_valid(adc_i_valid),.adc_q_valid(adc_q_valid),
        .config_word(config_word),.config_update(config_update),.fft_resetn(fft_resetn),
        .fft_config_data(fft_config_data),.fft_config_valid(fft_config_valid),.fft_config_ready(fft_config_ready),
        .fft_input_data(fft_input_data),.fft_input_valid(fft_input_valid),.fft_input_last(fft_input_last),
        .fft_input_ready(fft_input_ready),.fft_output_data(fft_output_data),
        .fft_output_valid(fft_output_valid),.fft_output_last(fft_output_last),
        .fft_output_user(fft_output_user),.fft_events(fft_events),.power_valid(power_valid),
        .power_data(power_data),.power_last(power_last),.power_index(power_index),.quality(quality),
        .frame_count(frame_count),.sample_counter(sample_counter),.status(status),
        .actual_log_n(actual_log_n),.fft_right_shift(fft_right_shift));
endmodule
