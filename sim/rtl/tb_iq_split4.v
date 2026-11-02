`timescale 1ns / 1ps
module tb_iq_split4;
    reg [63:0] i_data, q_data;
    wire signed [15:0] I0,I1,I2,I3,Q0,Q1,Q2,Q3;
    adc_iq_split4 dut(i_data,q_data,I0,I1,I2,I3,Q0,Q1,Q2,Q3);
    initial begin
        i_data=64'h7fff_8000_ffff_0001;
        q_data=64'h8001_0000_1234_abcd;
        #1;
        if (I0!==16'h0001 || I1!==16'hffff || I2!==16'h8000 || I3!==16'h7fff ||
            Q0!==16'habcd || Q1!==16'h1234 || Q2!==16'h0000 || Q3!==16'h8001)
            begin $display("ERROR: assertion failed");$display("Lane ordering mismatch");$finish;end
        if (I1 !== -16'sd1 || I2 !== -16'sd32768) begin $display("ERROR: assertion failed");$display("Signed data mismatch");$finish;end
        $display("IQ_SPLIT4_TEST_PASS");
        $finish;
    end
endmodule
