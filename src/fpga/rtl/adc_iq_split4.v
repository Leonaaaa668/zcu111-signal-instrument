`timescale 1ns / 1ps
// RFDC dual-ADC: separate 64-bit I and Q streams, oldest sample in low bits.
module adc_iq_split4 (
    input wire [63:0] i_data,
    input wire [63:0] q_data,
    output wire signed [15:0] I0, I1, I2, I3,
    output wire signed [15:0] Q0, Q1, Q2, Q3
);
    assign I0 = i_data[15:0];
    assign I1 = i_data[31:16];
    assign I2 = i_data[47:32];
    assign I3 = i_data[63:48];
    assign Q0 = q_data[15:0];
    assign Q1 = q_data[31:16];
    assign Q2 = q_data[47:32];
    assign Q3 = q_data[63:48];
endmodule
