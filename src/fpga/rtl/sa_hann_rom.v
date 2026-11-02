`timescale 1ns/1ps
// Keep the synchronous ROM read boundary separate from multiplier absorption.
(* keep_hierarchy="yes" *)
module sa_hann_rom #(parameter HANN_FILE="hann_q17.mem")(
    input wire clk, input wire enable, input wire [15:0] address,
    output reg [17:0] data
);
    (* rom_style="block" *) reg [17:0] rom [0:65535];
    initial $readmemh(HANN_FILE,rom);
    always @(posedge clk) begin
        if(enable) data<=rom[address];
    end
endmodule
