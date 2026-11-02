`timescale 1ns/1ps
// AXI-Lite independent AW/W queues and atomic XPM command/response mailboxes.
module sa_result_control(
    input wire aclk,input wire aresetn,input wire dsp_clk,input wire dsp_resetn,
    input wire [11:0] awaddr,input wire awvalid,output wire awready,
    input wire [31:0] wdata,input wire [3:0] wstrb,input wire wvalid,output wire wready,
    output reg [1:0] bresp,output reg bvalid,input wire bready,
    input wire [11:0] araddr,input wire arvalid,output wire arready,
    output reg [31:0] rdata,output reg [1:0] rresp,output reg rvalid,input wire rready,
    output wire pipe_resetn,output wire dsp_cancel,
    output wire command_valid,output wire [511:0] command_meta,input wire command_ready,
    input wire response_valid,input wire [191:0] response_data,output wire response_ready,
    input wire [4:0] live_status
);
    reg hold_reset,cancel_capture,request_busy,response_done;
    reg [31:0] shadow[0:15];reg [191:0] last_response;
    reg [31:0] active_ticket;
    reg address_pending,data_pending;reg [11:0] saved_address;
    reg [31:0] saved_data;reg [3:0] saved_strobe;
    reg [511:0] command_shadow;reg command_send;
    wire command_received,command_pulse;wire [511:0] command_bus;
    reg command_pending;reg [511:0] pending_meta;
    reg response_send;reg [191:0] response_shadow;reg [1:0] response_state;
    wire response_received,response_pulse;wire [191:0] response_bus;
    wire [4:0] live_sync;
    assign pipe_resetn=aresetn && !hold_reset;
    assign awready=!address_pending&&!bvalid;
    assign wready=!data_pending&&!bvalid;
    assign arready=!rvalid;
    assign command_valid=command_pending;
    assign command_meta=pending_meta;
    assign response_ready=(response_state==0 && !response_received && dsp_resetn);
    xpm_cdc_handshake #(.WIDTH(512),.DEST_EXT_HSK(0),.DEST_SYNC_FF(4),.SRC_SYNC_FF(4),
        .INIT_SYNC_FF(1),.SIM_ASSERT_CHK(1)) command_cdc(
        .src_clk(aclk),.src_in(command_shadow),.src_send(command_send),.src_rcv(command_received),
        .dest_clk(dsp_clk),.dest_out(command_bus),.dest_req(command_pulse),.dest_ack(1'b0));
    xpm_cdc_handshake #(.WIDTH(192),.DEST_EXT_HSK(0),.DEST_SYNC_FF(4),.SRC_SYNC_FF(4),
        .INIT_SYNC_FF(1),.SIM_ASSERT_CHK(1)) response_cdc(
        .src_clk(dsp_clk),.src_in(response_shadow),.src_send(response_send),.src_rcv(response_received),
        .dest_clk(aclk),.dest_out(response_bus),.dest_req(response_pulse),.dest_ack(1'b0));
    xpm_cdc_single #(.DEST_SYNC_FF(4),.SRC_INPUT_REG(1),.INIT_SYNC_FF(1)) cancel_cdc(
        .src_clk(aclk),.src_in(cancel_capture),.dest_clk(dsp_clk),.dest_out(dsp_cancel));
    xpm_cdc_array_single #(.WIDTH(5),.DEST_SYNC_FF(4),.SRC_INPUT_REG(1),
        .INIT_SYNC_FF(1),.SIM_ASSERT_CHK(1)) live_cdc(
        .src_clk(dsp_clk),.src_in(live_status),.dest_clk(aclk),.dest_out(live_sync));
    always @(posedge dsp_clk)begin
        if(!dsp_resetn)begin
            command_pending<=0;pending_meta<=0;response_send<=0;response_shadow<=0;response_state<=0;
        end else begin
            if(command_pulse)begin command_pending<=1;pending_meta<=command_bus;end
            else if(command_pending && command_ready)command_pending<=0;
            case(response_state)
                0:if(response_valid && response_ready)begin response_shadow<=response_data;response_send<=1;response_state<=1;end
                1:if(response_received)begin response_send<=0;response_state<=2;end
                2:if(!response_received)response_state<=0;
                default:response_state<=0;
            endcase
        end
    end
    function [31:0] merge_bytes;
        input [31:0] old_value,new_value;input [3:0] mask;
        integer b;begin
            merge_bytes=old_value;
            for(b=0;b<4;b=b+1)if(mask[b])merge_bytes[b*8+:8]=new_value[b*8+:8];
        end
    endfunction
    wire [31:0] merged_control=merge_bytes({30'd0,cancel_capture,hold_reset},saved_data,saved_strobe);
    wire go=saved_strobe[1] && saved_data[8];
    integer k;
    always @(posedge aclk)begin
        if(!aresetn)begin
            hold_reset<=1;cancel_capture<=0;request_busy<=0;response_done<=0;
            command_shadow<=0;command_send<=0;last_response<=0;active_ticket<=0;
            address_pending<=0;data_pending<=0;saved_address<=0;saved_data<=0;saved_strobe<=0;
            bresp<=0;bvalid<=0;rdata<=0;rresp<=0;rvalid<=0;
            for(k=0;k<16;k=k+1)shadow[k]<=0;
        end else begin
            if(command_send && command_received)command_send<=0;
            if(response_pulse && request_busy && !hold_reset && response_bus[63:32]==active_ticket)begin
                last_response<=response_bus;request_busy<=0;response_done<=1;
            end
            if(awready && awvalid)begin address_pending<=1;saved_address<=awaddr;end
            if(wready && wvalid)begin data_pending<=1;saved_data<=wdata;saved_strobe<=wstrb;end
            if(bvalid && bready)bvalid<=0;
            if(address_pending && data_pending && !bvalid)begin
                address_pending<=0;data_pending<=0;bvalid<=1;bresp<=0;
                if(saved_address[1:0]!=0)bresp<=2;
                else if(saved_address==0)begin
                    if(go && (request_busy || command_send || command_received || response_pulse ||
                              merged_control[1:0]!=0 || hold_reset || !live_sync[4]))bresp<=2;
                    else begin
                        hold_reset<=merged_control[0];cancel_capture<=merged_control[1];
                        if(merged_control[0])begin request_busy<=0;response_done<=0;command_send<=0;end
                        else if(go)begin
                            for(k=0;k<16;k=k+1)command_shadow[k*32+:32]<=shadow[k];
                            active_ticket<=shadow[3];command_send<=1;request_busy<=1;response_done<=0;
                        end
                    end
                end else if(saved_address>=12'h100 && saved_address<12'h140)begin
                    if(request_busy)bresp<=2;
                    else shadow[saved_address[5:2]]<=merge_bytes(shadow[saved_address[5:2]],saved_data,saved_strobe);
                end else bresp<=2;
            end
            if(rvalid && rready)rvalid<=0;
            if(arready && arvalid)begin
                rvalid<=1;rresp<=0;rdata<=0;
                if(araddr[1:0]!=0)rresp<=2;
                else case(araddr)
                    12'h000:rdata<={30'd0,cancel_capture,hold_reset};
                    12'h004:rdata<={19'd0,live_sync,5'd0,response_done,request_busy,(live_sync[4]&&!hold_reset)};
                    12'h008:rdata<=32'h31535852;
                    12'h00c:rdata<=last_response[31:0];12'h010:rdata<=last_response[63:32];
                    12'h014:rdata<=last_response[95:64];12'h018:rdata<=last_response[127:96];
                    12'h01c:rdata<=last_response[159:128];12'h020:rdata<=last_response[191:160];
                    default:if(araddr>=12'h100 && araddr<12'h140)rdata<=shadow[araddr[5:2]];
                            else rresp<=2;
                endcase
            end
        end
    end
endmodule
