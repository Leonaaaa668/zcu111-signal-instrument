`timescale 1ns/1ps
module tx_reset_sync (
    input wire clk,
    input wire parent_resetn,
    input wire [31:0] control,
    output wire resetn
);
    wire assert_reset = !parent_resetn || control[1];
    (* ASYNC_REG = "TRUE" *) reg [2:0] release_sync = 0;
    always @(posedge clk or posedge assert_reset)
        if (assert_reset) release_sync <= 0;
        else release_sync <= {release_sync[1:0],1'b1};
    assign resetn = release_sync[2];
endmodule
