`timescale 1ns/1ps
// Natural FFT-bin index and TLAST are pipelined with exact unsigned I^2+Q^2.
module sa_fft_power(
    input wire clk,input wire rst_n,input wire [4:0] log_n,
    input wire in_valid,input wire [47:0] in_data,input wire in_last,
    input wire [15:0] in_index,input wire in_overflow,
    output reg out_valid,output reg [63:0] out_power,
    output reg out_last,output reg [15:0] out_index,
    output reg [31:0] quality_sticky,output reg [63:0] completed_frames
);
    reg [47:0] square_i,square_q;
    reg [15:0] index0,expected_index;
    reg valid0,last0;
    wire signed [23:0] i=$signed(in_data[23:0]);
    wire signed [23:0] q=$signed(in_data[47:24]);
    wire [16:0] count=17'd1<<log_n;
    always @(posedge clk) begin
        if(!rst_n) begin
            valid0<=0;out_valid<=0;out_power<=0;out_last<=0;out_index<=0;
            expected_index<=0;quality_sticky<=0;completed_frames<=0;last0<=0;index0<=0;
        end else begin
            valid0<=in_valid;out_valid<=valid0;
            if(in_valid) begin
                square_i<=i*i;square_q<=q*q;last0<=in_last;index0<=in_index;
                if(in_overflow) quality_sticky[0]<=1;
                if(in_index!=expected_index) quality_sticky[1]<=1;
                if(in_last!=({1'b0,expected_index}==count-1)) quality_sticky[2]<=1;
                expected_index<=({1'b0,expected_index}==count-1) ? 0:expected_index+1;
            end
            if(valid0) begin
                out_power<={16'd0,square_i}+{16'd0,square_q};
                out_last<=last0;out_index<=index0;
                if(last0) completed_frames<=completed_frames+1;
            end
        end
    end
endmodule
