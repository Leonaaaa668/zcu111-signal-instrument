`timescale 1ns/1ps
// Bundled32-bit configuration uses the vendor-recognized atomic handshake CDC.
// A transaction restarts RX only; does not alter TX/DAC or RFDC registers.
module sa_rx_control(
    input wire ctrl_clk,input wire ctrl_resetn,input wire [31:0] control,
    input wire dsp_clk,input wire [31:0] dsp_status,
    output wire [31:0] status,
    output wire [31:0] config_word,output wire config_update
);
    reg [31:0] snapshot,last_sent;
    reg send;
    reg [1:0] state;
    wire received;
    always @(posedge ctrl_clk) begin
        if(!ctrl_resetn)begin snapshot<=32'h1000;last_sent<=32'hffffffff;send<=0;state<=0;end
        else case(state)
            0:if(control!=last_sent)begin snapshot<=control;last_sent<=control;send<=1;state<=1;end
            1:if(received)begin send<=0;state<=2;end
            2:if(!received)state<=0;
            default:state<=0;
        endcase
    end
    xpm_cdc_handshake #(.WIDTH(32),.DEST_EXT_HSK(0),.DEST_SYNC_FF(4),.SRC_SYNC_FF(4),
        .INIT_SYNC_FF(1),.SIM_ASSERT_CHK(1)) configuration_cdc(
        .src_clk(ctrl_clk),.src_in(snapshot),.src_send(send),.src_rcv(received),
        .dest_clk(dsp_clk),.dest_out(config_word),.dest_req(config_update),.dest_ack(1'b0));
    // Independent status flags and stable configuration echo; NOT an atomic
    // multi-counter snapshot. Detailed samples/frames will travel in packets.
    xpm_cdc_array_single #(.WIDTH(32),.DEST_SYNC_FF(4),.SRC_INPUT_REG(1),
        .INIT_SYNC_FF(1),.SIM_ASSERT_CHK(1)) status_cdc(
        .src_clk(dsp_clk),.src_in(dsp_status),.dest_clk(ctrl_clk),.dest_out(status));
endmodule
