`timescale 1ns/1ps
// One full-frame bank, split into <=4096-word pages. XPM supplies a registered
// two-cycle read and forbids the long BRAM cascade that missed 300MHz timing.
(* keep_hierarchy="yes" *)
module sa_snapshot_ram #(parameter integer LOG_DEPTH=16)(
    input wire clk,input wire write_enable,input wire [LOG_DEPTH-1:0] write_address,
    input wire [63:0] write_data,input wire read_enable,input wire [LOG_DEPTH-1:0] read_address,
    output wire [63:0] read_data
);
    localparam integer ROW_BITS=(LOG_DEPTH>12)?12:LOG_DEPTH;
    localparam integer PAGES=(1<<(LOG_DEPTH-ROW_BITS));
    localparam integer PAGE_BITS=(LOG_DEPTH>ROW_BITS)?LOG_DEPTH-ROW_BITS:1;
    wire [PAGE_BITS-1:0] write_page=write_address>>ROW_BITS;
    wire [PAGE_BITS-1:0] read_page=read_address>>ROW_BITS;
    wire [PAGES*64-1:0] page_data;
    // Delay payload/address with the page write enable. Only sixteen local
    // enable registers see the live capture decision, rather than every BRAM.
    // Capture validation leaves four tail cycles before publishing a bank.
    (* max_fanout=32 *) reg [ROW_BITS-1:0] write_row_q;
    (* max_fanout=32 *) reg [63:0] write_data_q;
    always @(posedge clk)begin
        write_row_q<=write_address[ROW_BITS-1:0];
        write_data_q<=write_data;
    end
    assign read_data=page_data[read_page*64+:64];
    genvar page;
    generate for(page=0;page<PAGES;page=page+1)begin:pages
        (* max_fanout=16 *) reg wen=1'b0;
        always @(posedge clk)wen<=write_enable && write_page==page;
        wire ren=read_enable && read_page==page;
        xpm_memory_sdpram #(.MEMORY_SIZE(64*(1<<ROW_BITS)),.MEMORY_PRIMITIVE("block"),
            .CLOCKING_MODE("common_clock"),.ECC_MODE("no_ecc"),.MEMORY_INIT_FILE("none"),
            .MEMORY_INIT_PARAM("0"),.USE_MEM_INIT(0),.CASCADE_HEIGHT(1),.SIM_ASSERT_CHK(1),
            .WRITE_DATA_WIDTH_A(64),.BYTE_WRITE_WIDTH_A(64),.ADDR_WIDTH_A(ROW_BITS),
            .READ_DATA_WIDTH_B(64),.ADDR_WIDTH_B(ROW_BITS),.READ_LATENCY_B(2),
            .WRITE_MODE_B("read_first"),.RST_MODE_B("SYNC"),.READ_RESET_VALUE_B("0")) ram(
            .clka(clk),.ena(wen),.wea(wen),.addra(write_row_q),.dina(write_data_q),
            .clkb(clk),.rstb(1'b0),.enb(ren),.regceb(1'b1),.addrb(read_address[ROW_BITS-1:0]),
            .doutb(page_data[page*64+:64]),.sleep(1'b0),.injectsbiterra(1'b0),.injectdbiterra(1'b0),
            .sbiterrb(),.dbiterrb());
    end endgenerate
endmodule
