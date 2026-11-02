`timescale 1ns/1ps
// All filters share the original ADC 300MHz clock; valid expresses sample rate.
module sa_d8_zoom #(
    parameter COEFF0="d2_1200_to600.mem",
    parameter COEFF1="d2_600_to300.mem",
    parameter COEFF2="d2_300_to150.mem",
    parameter COEFF_ZOOM="zoom_d2_normalized.mem",
    parameter integer MAX_ZOOM=6
)(
    input wire clk, input wire rst_n,
    input wire [63:0] adc_i, input wire [63:0] adc_q,
    input wire adc_i_valid, input wire adc_q_valid,
    input wire monitor_enable,
    input wire [2:0] zoom_stages,
    output wire signed [23:0] out_i, output wire signed [23:0] out_q,
    output wire out_valid,
    output reg [31:0] quality_sticky,
    output reg [63:0] adc_sample_counter
);
    wire [47:0] i2,q2;
    wire [23:0] i1,q1;
    wire v2,v1,s0,s1,s2;
    wire [23:0] zi[0:6], zq[0:6];
    wire [6:0] zv;
    wire [5:0] zs;
    sa_halfband_decim2 #(.COEFF_FILE(COEFF0)) d0(
        .clk(clk),.rst_n(rst_n),.in_i(adc_i),.in_q(adc_q),.in_valid(adc_i_valid&&adc_q_valid),
        .out_i(i2),.out_q(q2),.out_valid(v2),.out_saturated(s0));
    sa_halfband_decim2 #(.IN_LANES(2),.IN_WIDTH(24),.TAPS(23),.SHIFT(17),.COEFF_FILE(COEFF1)) d1(
        .clk(clk),.rst_n(rst_n),.in_i(i2),.in_q(q2),.in_valid(v2),
        .out_i(i1),.out_q(q1),.out_valid(v1),.out_saturated(s1));
    sa_halfband_decim2 #(.IN_LANES(1),.IN_WIDTH(24),.TAPS(63),.SHIFT(17),.COEFF_FILE(COEFF2)) d2(
        .clk(clk),.rst_n(rst_n),.in_i(i1),.in_q(q1),.in_valid(v1),
        .out_i(zi[0]),.out_q(zq[0]),.out_valid(zv[0]),.out_saturated(s2));
    genvar stage;
    generate for(stage=0;stage<6;stage=stage+1) begin:g_zoom
      if(stage<MAX_ZOOM) begin:g_enabled
        sa_halfband_decim2 #(.IN_LANES(1),.IN_WIDTH(24),.TAPS(63),.SHIFT(17),.COEFF_FILE(COEFF_ZOOM)) z(
            .clk(clk),.rst_n(rst_n),.in_i(zi[stage]),.in_q(zq[stage]),
            .in_valid(zv[stage] && zoom_stages>stage),
            .out_i(zi[stage+1]),.out_q(zq[stage+1]),.out_valid(zv[stage+1]),.out_saturated(zs[stage]));
      end else begin:g_disabled
        assign zi[stage+1]=24'd0;
        assign zq[stage+1]=24'd0;
        assign zv[stage+1]=1'b0;
        assign zs[stage]=1'b0;
      end
    end endgenerate
    assign out_i = zoom_stages<=MAX_ZOOM ? zi[zoom_stages] : 24'sd0;
    assign out_q = zoom_stages<=MAX_ZOOM ? zq[zoom_stages] : 24'sd0;
    assign out_valid = zoom_stages<=MAX_ZOOM && zv[zoom_stages];
    always @(posedge clk) begin
        if(!rst_n) begin quality_sticky<=0;adc_sample_counter<=0;end
        else begin
            adc_sample_counter<=adc_sample_counter+4;
            if(monitor_enable && !(adc_i_valid&&adc_q_valid)) quality_sticky[0]<=1;
            if(monitor_enable && (adc_i_valid^adc_q_valid)) quality_sticky[1]<=1;
            if(s0||s1||s2||(|zs)) quality_sticky[2]<=1;
            if(zoom_stages>MAX_ZOOM) quality_sticky[3]<=1;
        end
    end
endmodule
