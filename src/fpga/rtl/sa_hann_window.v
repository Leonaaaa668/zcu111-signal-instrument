`timescale 1ns/1ps
// Periodic Hann, maximum65536 coefficients, runtime N=128..65536.
// Input/output: signed24, coefficient signed18 Q1.17, ties-to-even.
module sa_hann_window #(parameter HANN_FILE="hann_q17.mem")(
    input wire clk, input wire rst_n, input wire [4:0] log_n,
    input wire in_valid, input wire signed [23:0] in_i, input wire signed [23:0] in_q,
    output reg out_valid, output reg [47:0] out_data,
    output reg out_last, output reg [15:0] out_index
);
    reg [15:0] index;
    wire [15:0] rom_address=index << (16-log_n);
    wire [16:0] count=(17'd1 << log_n);
    wire signed [17:0] coeff;
    reg signed [23:0] i0,q0,i1,q1;
    reg signed [17:0] coeff1;
    reg signed [41:0] pi,pq;
    reg v0,v1,v2,last0,last1,last2;
    reg [15:0] index0,index1,index2;
    sa_hann_rom #(.HANN_FILE(HANN_FILE)) coefficient_rom(
        .clk(clk),.enable(in_valid),.address(rom_address),.data(coeff));
    function automatic [23:0] rounded;
        input signed [41:0] value;
        reg signed [41:0] base,remainder;
        begin
            base=value>>>17;
            remainder=value-(base<<<17);
            if(remainder>65536 || (remainder==65536 && base[0])) base=base+1;
            rounded=base[23:0];
        end
    endfunction
    always @(posedge clk) begin
        if(!rst_n) begin
            index<=0;v0<=0;v1<=0;v2<=0;out_valid<=0;out_last<=0;out_data<=0;out_index<=0;
            last0<=0;last1<=0;last2<=0;index0<=0;index1<=0;index2<=0;
        end else begin
            v0<=in_valid;v1<=v0;v2<=v1;out_valid<=v2;
            if(in_valid) begin
                i0<=in_i;q0<=in_q;
                last0<=({1'b0,index}==count-1); index0<=index;
                index<=({1'b0,index}==count-1) ? 0:index+1;
            end
            // Split BRAM output/multiplication into separate clock stages.
            // The sample, coefficient, validity and frame tags travel together.
            if(v0) begin i1<=i0;q1<=q0;coeff1<=coeff;last1<=last0;index1<=index0;end
            if(v1) begin pi<=i1*coeff1;pq<=q1*coeff1;last2<=last1;index2<=index1;end
            if(v2) begin out_data<={rounded(pq),rounded(pi)};out_last<=last2;out_index<=index2;end
        end
    end
endmodule
