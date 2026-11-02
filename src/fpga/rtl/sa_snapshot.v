`timescale 1ns/1ps
// Whole-frame dual-bank capture. Never backpressures the live FFT power source.
module sa_snapshot #(parameter integer MAX_LOG_N=16)(
    input wire clk,input wire rst_n,input wire cancel,input wire config_update,
    input wire request_valid,input wire [511:0] request_meta,output wire request_ready,
    output reg response_valid,output reg [191:0] response_data,input wire response_ready,
    input wire power_valid,input wire [63:0] power_data,input wire [15:0] power_index,input wire power_last,
    input wire [31:0] quality,input wire [63:0] frame_count,input wire [63:0] sample_counter,
    input wire [31:0] dsp_status,input wire [4:0] log_n,input wire [4:0] right_shift,
    output reg [63:0] m_axis_tdata,output wire [7:0] m_axis_tkeep,
    output reg m_axis_tvalid,output reg m_axis_tlast,input wire m_axis_tready,
    output wire [4:0] live_status
);
    `include "sa_window_metrics.vh"
    reg [511:0] bank_meta[0:1];
    reg [4:0] bank_logn[0:1],bank_shift[0:1];
    reg [15:0] bank_last_index[0:1];
    reg [2:0] bank_zoom[0:1];
    reg [63:0] bank_frame[0:1],bank_start[0:1],bank_end[0:1],bank_xor[0:1],bank_sequence[0:1];
    reg [31:0] bank_status[0:1],bank_drops[0:1];
    reg [1:0] owned,available;
    reg [63:0] sequence_counter;
    reg [31:0] drops;
    localparam C_IDLE=0,C_WAIT=1,C_BODY=2,C_VALIDATE=3,C_DONE=4;
    reg [2:0] capture_state;
    reg capture_bank;
    reg [511:0] ticket;
    reg [16:0] expected_index;
    reg [15:0] cap_last_index;
    reg [63:0] cap_frame_count,cap_next_frame;
    reg [4:0] cap_logn;
    reg [2:0] grace;
    reg [31:0] completion_code;
    reg [63:0] completion_sequence;
    wire [2:0] zoom=dsp_status[20:18];
    wire source_ready=(dsp_status[3:0]==4'hf && !dsp_status[4]);
    // Word13 is the original GPIO control: commit31/logN12:8/zoom18:16.
    wire match_request=(dsp_status[31]==request_meta[447] && log_n==request_meta[428:424] && zoom==request_meta[434:432]);
    wire match_active=(dsp_status[31]==ticket[447] && log_n==ticket[428:424] && zoom==ticket[434:432]);
    wire power_range_ok=(power_data<=64'h0000800000000000);
    // Precompute frame geometry and next-frame count before the body arrives.
    // Do not chain N-1, an end-of-frame decision, and a 64-bit add/compare.
    wire at_last_sample=(expected_index=={1'b0,cap_last_index});
    wire matches_body_frame=(frame_count==cap_frame_count);
    wire matches_last_frame=(frame_count==cap_next_frame);
    wire frame_sequence_ok=at_last_sample?matches_last_frame:matches_body_frame;
    assign request_ready=!response_valid;
    assign m_axis_tkeep=8'hff;
    localparam S_IDLE=0,S_HEADER=1,S_READ=2,S_WAIT=3,S_LATCH=4,S_EMIT=5,S_LAST=6;
    reg [2:0] send_state;
    reg send_bank;
    reg [4:0] header_index;
    reg [16:0] read_index;
    wire [63:0] read_data0,read_data1;
    reg [63:0] selected_data;
    assign live_status={rst_n,(capture_state!=C_IDLE), (send_state!=S_IDLE),owned};
    wire slot_free=!m_axis_tvalid || m_axis_tready;
    wire sending_last_sample=(read_index=={1'b0,bank_last_index[send_bank]});
    wire reading=(send_state==S_READ && slot_free);
    wire writing=(capture_state==C_BODY && power_valid) ||
                 (capture_state==C_WAIT && power_valid && power_index==0);
    wire [MAX_LOG_N-1:0] write_address=(capture_state==C_WAIT)?0:expected_index[MAX_LOG_N-1:0];
    sa_snapshot_ram #(.LOG_DEPTH(MAX_LOG_N)) memory0(.clk(clk),
        .write_enable(rst_n && writing && !capture_bank),.write_address(write_address),.write_data(power_data),
        .read_enable(rst_n && reading && !send_bank),.read_address(read_index[MAX_LOG_N-1:0]),.read_data(read_data0));
    sa_snapshot_ram #(.LOG_DEPTH(MAX_LOG_N)) memory1(.clk(clk),
        .write_enable(rst_n && writing && capture_bank),.write_address(write_address),.write_data(power_data),
        .read_enable(rst_n && reading && send_bank),.read_address(read_index[MAX_LOG_N-1:0]),.read_data(read_data1));
    function [31:0] header_word;
        input [5:0] word_index;
        reg [511:0] m;reg [63:0] sumw,sumsq;reg [16:0] n;
        begin
            m=bank_meta[send_bank];n=17'd1<<bank_logn[send_bank];
            sumw=window_sum(bank_logn[send_bank]);sumsq=window_sum_square(bank_logn[send_bank]);
            header_word=0;
            case(word_index)
                0:header_word=32'h31504153;1:header_word=1;2:header_word=256;
                3:header_word=256+({15'd0,n}<<3);4:header_word=32'h3f;
                5:header_word=bank_sequence[send_bank][31:0];6:header_word=bank_sequence[send_bank][63:32];
                7:header_word=m[31:0];8:header_word=m[63:32];9:header_word=m[95:64];10:header_word=m[127:96];
                11:header_word={15'd0,n};12:header_word={27'd0,bank_logn[send_bank]};
                13:header_word={29'd0,bank_zoom[send_bank]};14:header_word={27'd0,bank_shift[send_bank]};
                15:header_word=m[415:384];16:header_word=m[351:320];17:header_word=m[383:352];
                18:header_word=32'd150000000>>bank_zoom[send_bank];19:header_word=32'd1200000000;
                20:header_word=m[159:128];21:header_word=m[191:160];
                22:header_word=m[223:192];23:header_word=m[255:224];
                24:header_word=bank_frame[send_bank][31:0];25:header_word=bank_frame[send_bank][63:32];
                26:header_word=bank_start[send_bank][31:0];27:header_word=bank_start[send_bank][63:32];
                28:header_word=bank_end[send_bank][31:0];29:header_word=bank_end[send_bank][63:32];
                30:header_word=m[287:256];31:header_word=m[319:288];
                32:header_word=0;33:header_word=bank_drops[send_bank];
                34:header_word=bank_xor[send_bank][31:0];35:header_word=bank_xor[send_bank][63:32];
                36:header_word=bank_status[send_bank];37:header_word=1;
                40:header_word=sumw[31:0];41:header_word=sumw[63:32];
                42:header_word=sumsq[31:0];43:header_word=sumsq[63:32];
                44:header_word=17;45:header_word=8;46:header_word=24;47:header_word=16;48:header_word=1;
                56:header_word={WINDOW_MEM_SHA256[231:224],WINDOW_MEM_SHA256[239:232],WINDOW_MEM_SHA256[247:240],WINDOW_MEM_SHA256[255:248]};
                57:header_word={WINDOW_MEM_SHA256[199:192],WINDOW_MEM_SHA256[207:200],WINDOW_MEM_SHA256[215:208],WINDOW_MEM_SHA256[223:216]};
                58:header_word={WINDOW_MEM_SHA256[167:160],WINDOW_MEM_SHA256[175:168],WINDOW_MEM_SHA256[183:176],WINDOW_MEM_SHA256[191:184]};
                59:header_word={WINDOW_MEM_SHA256[135:128],WINDOW_MEM_SHA256[143:136],WINDOW_MEM_SHA256[151:144],WINDOW_MEM_SHA256[159:152]};
                60:header_word={WINDOW_MEM_SHA256[103:96],WINDOW_MEM_SHA256[111:104],WINDOW_MEM_SHA256[119:112],WINDOW_MEM_SHA256[127:120]};
                61:header_word={WINDOW_MEM_SHA256[71:64],WINDOW_MEM_SHA256[79:72],WINDOW_MEM_SHA256[87:80],WINDOW_MEM_SHA256[95:88]};
                62:header_word={WINDOW_MEM_SHA256[39:32],WINDOW_MEM_SHA256[47:40],WINDOW_MEM_SHA256[55:48],WINDOW_MEM_SHA256[63:56]};
                63:header_word={WINDOW_MEM_SHA256[7:0],WINDOW_MEM_SHA256[15:8],WINDOW_MEM_SHA256[23:16],WINDOW_MEM_SHA256[31:24]};
                default:header_word=0;
            endcase
        end
    endfunction
    integer k;
    always @(posedge clk)begin
        if(!rst_n)begin
            owned<=0;available<=0;sequence_counter<=0;drops<=0;capture_state<=C_IDLE;
            response_valid<=0;response_data<=0;capture_bank<=0;ticket<=0;expected_index<=0;cap_last_index<=0;cap_logn<=0;
            cap_frame_count<=0;cap_next_frame<=0;
            grace<=0;completion_code<=0;completion_sequence<=0;
            send_state<=S_IDLE;send_bank<=0;header_index<=0;read_index<=0;
            m_axis_tvalid<=0;m_axis_tlast<=0;m_axis_tdata<=0;selected_data<=0;
            for(k=0;k<2;k=k+1)begin
                bank_meta[k]<=0;bank_logn[k]<=0;bank_shift[k]<=0;bank_zoom[k]<=0;bank_last_index[k]<=0;
                bank_frame[k]<=0;bank_start[k]<=0;bank_end[k]<=0;bank_xor[k]<=0;
                bank_sequence[k]<=0;bank_status[k]<=0;bank_drops[k]<=0;
            end
        end else begin
            if(response_valid && response_ready)response_valid<=0;
            if(request_valid && request_ready)begin
                if(capture_state!=C_IDLE || &owned || cancel || !source_ready || quality!=0 ||
                   log_n<7 || log_n>MAX_LOG_N || right_shift!=log_n+1 || !match_request ||
                   request_meta[511:480]!=32'h52585331 || request_meta[351:320]!=32'd2400000000 ||
                   request_meta[383:352]!=32'd1200000000 || request_meta[415:384]!=1 ||
                   request_meta[255:240]!=0 || request_meta[479:448]!=0 ||
                   !request_meta[416] || request_meta[417])begin
                    drops<=drops+1;response_valid<=1;
                    response_data<={request_meta[31:0],64'd0,(drops+32'd1),request_meta[127:96],
                        (cancel?32'd7:(quality!=0?32'd5:(!source_ready?32'd3:
                         ((capture_state!=C_IDLE || &owned)?32'd2:32'd4))))};
                end else begin
                    capture_bank<=owned[0];owned[owned[0]]<=1;ticket<=request_meta;
                    capture_state<=C_WAIT;cap_logn<=log_n;cap_last_index<=(17'd1<<log_n)-17'd1;completion_sequence<=0;
                end
            end
            if(capture_state==C_WAIT || capture_state==C_BODY || capture_state==C_VALIDATE)begin
                if(cancel || config_update || !source_ready || !match_active || quality!=0 || log_n!=cap_logn || right_shift!=log_n+1)begin
                    owned[capture_bank]<=0;capture_state<=C_DONE;drops<=drops+1;
                    completion_code<=cancel?7:((quality!=0)?5:4);
                end else case(capture_state)
                    C_WAIT:if(power_valid && power_index==0)begin
                        bank_meta[capture_bank]<=ticket;bank_logn[capture_bank]<=log_n;
                        bank_last_index[capture_bank]<=cap_last_index;
                        bank_zoom[capture_bank]<=zoom;bank_shift[capture_bank]<=right_shift;
                        bank_frame[capture_bank]<=frame_count;bank_start[capture_bank]<=sample_counter;
                        cap_frame_count<=frame_count;cap_next_frame<=frame_count+64'd1;
                        bank_status[capture_bank]<=dsp_status;bank_xor[capture_bank]<=power_data;
                        expected_index<=1;capture_state<=C_BODY;
                        if(power_last || !power_range_ok)begin owned[capture_bank]<=0;capture_state<=C_DONE;drops<=drops+1;completion_code<=6;end
                    end
                    C_BODY:if(power_valid)begin
                        bank_xor[capture_bank]<=bank_xor[capture_bank]^power_data;
                        expected_index<=expected_index+1;
                        if(!power_range_ok || power_index!=expected_index[15:0] || power_last!=at_last_sample ||
                           !frame_sequence_ok)begin
                            owned[capture_bank]<=0;capture_state<=C_DONE;drops<=drops+1;completion_code<=6;
                        end else if(at_last_sample)begin
                            bank_end[capture_bank]<=sample_counter;grace<=4;capture_state<=C_VALIDATE;
                        end
                    end
                    C_VALIDATE:if(grace!=0)grace<=grace-1;
                    else begin
                        sequence_counter<=sequence_counter+1;bank_sequence[capture_bank]<=sequence_counter+1;
                        bank_drops[capture_bank]<=drops;available[capture_bank]<=1;
                        completion_sequence<=sequence_counter+1;completion_code<=1;capture_state<=C_DONE;
                    end
                    default:begin end
                endcase
            end
            if(capture_state==C_DONE && !response_valid && !(request_valid && request_ready))begin
                response_valid<=1;response_data<={ticket[31:0],completion_sequence,drops,ticket[127:96],completion_code};
                capture_state<=C_IDLE;
            end
            if(m_axis_tvalid && m_axis_tready)m_axis_tvalid<=0;
            case(send_state)
                S_IDLE:if(|available)begin
                    if(available==2'b11)send_bank<=(bank_sequence[1]<bank_sequence[0]);
                    else send_bank<=available[1];
                    if(available==2'b11)available[(bank_sequence[1]<bank_sequence[0])]<=0;
                    else available[available[1]]<=0;
                    header_index<=0;read_index<=0;send_state<=S_HEADER;
                end
                S_HEADER:if(slot_free)begin
                    m_axis_tdata<={header_word({header_index,1'b1}),header_word({header_index,1'b0})};
                    m_axis_tvalid<=1;m_axis_tlast<=0;header_index<=header_index+1;
                    if(header_index==31)send_state<=S_READ;
                end
                S_READ:if(slot_free)send_state<=S_WAIT;
                S_WAIT:send_state<=S_LATCH;
                S_LATCH:begin selected_data<=send_bank?read_data1:read_data0;send_state<=S_EMIT;end
                S_EMIT:begin
                    m_axis_tdata<=selected_data;m_axis_tvalid<=1;
                    m_axis_tlast<=sending_last_sample;
                    read_index<=read_index+1;
                    send_state<=sending_last_sample?S_LAST:S_READ;
                end
                S_LAST:if(m_axis_tvalid && m_axis_tready)begin owned[send_bank]<=0;send_state<=S_IDLE;end
                default:send_state<=S_IDLE;
            endcase
        end
    end
endmodule
