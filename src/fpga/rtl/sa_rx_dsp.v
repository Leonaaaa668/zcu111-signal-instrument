`timescale 1ns/1ps
// Center-window RX DSP frontend. Output is raw power stream + explicit quality;
// frame snapshot/VBW/result-DMA packetizer remain separate downstream blocks.
module sa_rx_dsp #(
    parameter COEFF0="d2_1200_to600.mem",parameter COEFF1="d2_600_to300.mem",
    parameter COEFF2="d2_300_to150.mem",parameter COEFF_ZOOM="zoom_d2_normalized.mem",
    parameter HANN_FILE="hann_q17.mem",parameter integer PRIME_SAMPLES=1024,
    parameter integer MAX_ZOOM=6
)(
    input wire clk,input wire rst_n,
    input wire [63:0] adc_i,input wire [63:0] adc_q,input wire adc_i_valid,input wire adc_q_valid,
    input wire [31:0] config_word,input wire config_update,
    output wire fft_resetn,output reg [31:0] fft_config_data,output reg fft_config_valid,
    input wire fft_config_ready,
    output wire [47:0] fft_input_data,output wire fft_input_valid,output wire fft_input_last,
    input wire fft_input_ready,
    input wire [47:0] fft_output_data,input wire fft_output_valid,input wire fft_output_last,
    input wire [23:0] fft_output_user,input wire [5:0] fft_events,
    output wire power_valid,output wire [63:0] power_data,output wire power_last,
    output wire [15:0] power_index,output wire [31:0] quality,
    output wire [63:0] frame_count,output wire [63:0] sample_counter,output wire [31:0] status,
    output wire [4:0] actual_log_n,output wire [4:0] fft_right_shift
);
    reg [31:0] active;
    reg [5:0] reset_hold;
    wire [4:0] logn=active[12:8];
    wire [2:0] zoom=active[18:16];
    wire valid_config=logn>=7 && logn<=16 && zoom<=MAX_ZOOM;
    // Decode configuration BEFORE the reset-distribution register. Previously
    // active/hold decoding drove thousands of FIR/DSP reset pins directly.
    // Assert on the same configuration edge; release only after the full hold.
    // The extra release cycle is covered by priming, not by a timing exception.
    (* max_fanout=64 *) reg pipe_resetn=1'b0;
    always @(posedge clk) begin
        if (!rst_n || config_update) pipe_resetn <= 1'b0;
        else pipe_resetn <= active[0] && !active[1] && reset_hold==0 && valid_config;
    end
    assign fft_resetn=pipe_resetn;
    assign actual_log_n=logn;
    assign fft_right_shift=logn+1;
    wire signed [23:0]di,dq;
    wire dv,wv,wl,loss;
    wire [47:0]wd;wire [15:0]wi;
    wire [31:0]dqflags,pqflags,lost;
    reg [31:0] prime;
    reg configured;
    reg [3:0] config_wait;
    reg [5:0] events_seen;
    wire primed=prime>=PRIME_SAMPLES;
    wire accept_sample=dv && configured && config_wait==0 && primed;
    function [15:0] schedule;
        input [4:0] n;
        integer g;begin schedule=0;
        for(g=0;g<8;g=g+1)begin
            if(g==0)schedule[2*g+:2]=3;
            else if(2*g+1<n)schedule[2*g+:2]=2;
            else if(2*g<n)schedule[2*g+:2]=1;
        end end
    endfunction
    always@(posedge clk)begin
        if(!rst_n)begin active<=32'h1000;reset_hold<=32;end
        else if(config_update)begin active<=config_word;reset_hold<=32;end
        else if(reset_hold!=0)reset_hold<=reset_hold-1;
        if(!pipe_resetn)begin
            prime<=0;configured<=0;config_wait<=0;fft_config_valid<=0;fft_config_data<=0;events_seen<=0;
        end else begin
            if(!configured)begin
                fft_config_valid<=1;
                fft_config_data<={7'd0,schedule(logn),1'b1,3'd0,logn};
                if(fft_config_valid && fft_config_ready)begin
                    fft_config_valid<=0;configured<=1;config_wait<=8;
                end
            end
            if(config_wait!=0)config_wait<=config_wait-1;
            if(dv && !primed)prime<=prime+1;
            events_seen<=events_seen|fft_events;
        end
    end
    sa_d8_zoom #(.COEFF0(COEFF0),.COEFF1(COEFF1),.COEFF2(COEFF2),.COEFF_ZOOM(COEFF_ZOOM),.MAX_ZOOM(MAX_ZOOM)) decim(
        .clk(clk),.rst_n(pipe_resetn),.adc_i(adc_i),.adc_q(adc_q),.adc_i_valid(adc_i_valid),.adc_q_valid(adc_q_valid),
        .monitor_enable(configured),.zoom_stages(zoom),.out_i(di),.out_q(dq),.out_valid(dv),
        .quality_sticky(dqflags),.adc_sample_counter(sample_counter));
    sa_hann_window #(.HANN_FILE(HANN_FILE)) windowing(
        .clk(clk),.rst_n(pipe_resetn),.log_n(logn),.in_valid(accept_sample),.in_i(di),.in_q(dq),
        .out_valid(wv),.out_data(wd),.out_last(wl),.out_index(wi));
    sa_axis_loss_guard guard(.clk(clk),.rst_n(pipe_resetn),.source_valid(wv),.source_data(wd),.source_last(wl),
        .m_axis_tdata(fft_input_data),.m_axis_tvalid(fft_input_valid),.m_axis_tlast(fft_input_last),
        .m_axis_tready(fft_input_ready),.loss_sticky(loss),.lost_samples(lost));
    sa_fft_power power(.clk(clk),.rst_n(pipe_resetn),.log_n(logn),.in_valid(fft_output_valid),
        .in_data(fft_output_data),.in_last(fft_output_last),.in_index(fft_output_user[15:0]),.in_overflow(fft_output_user[16]),
        .out_valid(power_valid),.out_power(power_data),.out_last(power_last),.out_index(power_index),
        .quality_sticky(pqflags),.completed_frames(frame_count));
    assign quality={15'd0,loss,2'd0,events_seen,pqflags[3:0],dqflags[3:0]};
    assign status={active[31],10'd0,zoom,logn,8'd0,(|quality),valid_config,primed,configured,pipe_resetn};
endmodule
