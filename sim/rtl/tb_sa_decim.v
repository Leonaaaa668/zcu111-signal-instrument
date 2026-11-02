`timescale 1ns/1ps
module sa_decim_case #(
    parameter integer LANES=4, WIDTH=16, TAPS=15, SHIFT=9,
    parameter STEM="case0", COEFF="d2_1200_to600.mem",
    parameter integer OUT_LANES=(LANES==1 ? 1 : LANES/2),
    parameter integer INPUT_BEATS=1024/LANES,
    parameter integer OUTPUT_BEATS=512/OUT_LANES
)(output reg done=0);
    reg clk=0, rst_n=0, valid=0;
    reg [LANES*WIDTH-1:0] i=0, q=0;
    wire ov, saturated;
    wire [OUT_LANES*24-1:0] oi, oq;
    reg [LANES*WIDTH-1:0] vi[0:INPUT_BEATS-1], vq[0:INPUT_BEATS-1];
    reg [OUT_LANES*24-1:0] ei[0:OUTPUT_BEATS-1], eq[0:OUTPUT_BEATS-1];
    reg es[0:OUTPUT_BEATS-1];
    integer sent=0, received=0, cycle=0, flush_count=0;
    always #5 clk=~clk;
    sa_halfband_decim2 #(.IN_LANES(LANES), .IN_WIDTH(WIDTH), .TAPS(TAPS),
                        .SHIFT(SHIFT), .COEFF_FILE(COEFF)) dut(
        .clk(clk), .rst_n(rst_n), .in_valid(valid), .in_i(i), .in_q(q),
        .out_valid(ov), .out_i(oi), .out_q(oq), .out_saturated(saturated));
    initial begin
        $readmemh({STEM,"_i.mem"},vi); $readmemh({STEM,"_q.mem"},vq);
        $readmemh({STEM,"_ei.mem"},ei); $readmemh({STEM,"_eq.mem"},eq);
        $readmemh({STEM,"_s.mem"},es);
        repeat(4) @(negedge clk);
        rst_n=1;
        while (sent<INPUT_BEATS) begin
            @(negedge clk);
            cycle=cycle+1;
            valid=(cycle%7!=3 && cycle%11!=5);
            if(valid) begin i=vi[sent]; q=vq[sent]; sent=sent+1; end
        end
        @(negedge clk); valid=0;
        repeat(20) @(negedge clk);
        if(received!=OUTPUT_BEATS) begin $display("ERROR: %s count=%0d expected=%0d",STEM,received,OUTPUT_BEATS); $finish; end
        $display("SA_DECIM_CASE_PASS %s received=%0d",STEM,received);
        done=1;
    end
    always @(posedge clk) begin
        #1;
        if(rst_n && ov) begin
            if(received>=OUTPUT_BEATS) begin $display("ERROR: %s extra output",STEM); $finish; end
            if(oi!==ei[received] || oq!==eq[received] || saturated!==es[received])
                begin $display("ERROR: %s mismatch n=%0d i=%h/%h q=%h/%h sat=%b/%b",STEM,received,
                       oi,ei[received],oq,eq[received],saturated,es[received]); $finish; end
            received=received+1;
        end
    end
endmodule

module tb_sa_decim;
    wire d0,d1,d2;
    sa_decim_case #(.LANES(4), .WIDTH(16), .TAPS(15), .SHIFT(9), .STEM("case0"),
                    .COEFF("d2_1200_to600.mem")) c0(d0);
    sa_decim_case #(.LANES(2), .WIDTH(24), .TAPS(23), .SHIFT(17), .STEM("case1"),
                    .COEFF("d2_600_to300.mem")) c1(d1);
    sa_decim_case #(.LANES(1), .WIDTH(24), .TAPS(63), .SHIFT(17), .STEM("case2"),
                    .COEFF("d2_300_to150.mem")) c2(d2);
    initial begin
        wait(d0 && d1 && d2);
        $display("SA_DECIM_RTL_BIT_EXACT_PASS; NOT board timing/integration acceptance");
        $finish;
    end
    initial begin #100000; begin $display("ERROR: timeout"); $finish; end end
endmodule
