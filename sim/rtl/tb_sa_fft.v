`timescale 1ns/1ps
module tb_sa_fft;
    reg clk=0,rst_n=0;
    always #5 clk=~clk;
    reg [1023:0] filename;
    integer logn=7,n,sent=0,received=0,fd,k;
    reg cfgv=0,iv=0;
    wire cfgr,fr;
    reg[31:0]cfg;
    reg signed[23:0]i=0,q=0;
    reg[23:0]vi[0:65535],vq[0:65535];
    reg[47:0]raw[0:65535];
    wire wv,wl;wire[47:0]wd;wire[15:0]wi;
    wire av,al,loss;wire[47:0]ad;wire[31:0]lost;
    wire fv,fl;wire[47:0]fx;wire[23:0]fu;
    wire[7:0]status;wire status_valid;
    wire efs,etlu,etlm,eov,esh,eih,eoh;
    wire pv,pl;wire[63:0]power,frames;wire[15:0]pi;wire[31:0]quality;
    function [15:0] schedule;
        input integer stages;
        integer g;begin schedule=0;
        for(g=0;g<8;g=g+1)begin
            if(g==0)schedule[2*g+:2]=3;
            else if(2*g+1<stages)schedule[2*g+:2]=2;
            else if(2*g<stages)schedule[2*g+:2]=1;
        end end
    endfunction
    sa_hann_window window_dut(.clk(clk),.rst_n(rst_n),.log_n(logn[4:0]),.in_valid(iv),.in_i(i),.in_q(q),
        .out_valid(wv),.out_data(wd),.out_last(wl),.out_index(wi));
    sa_axis_loss_guard guard(.clk(clk),.rst_n(rst_n),.source_valid(wv),.source_data(wd),.source_last(wl),
        .m_axis_tvalid(av),.m_axis_tdata(ad),.m_axis_tlast(al),.m_axis_tready(fr),.loss_sticky(loss),.lost_samples(lost));
    adc3_capture_sa_fft_0 fft(.aclk(clk),.aresetn(rst_n),.s_axis_config_tdata(cfg),.s_axis_config_tvalid(cfgv),
        .s_axis_config_tready(cfgr),.s_axis_data_tdata(ad),.s_axis_data_tvalid(av),.s_axis_data_tready(fr),.s_axis_data_tlast(al),
        .m_axis_data_tdata(fx),.m_axis_data_tuser(fu),.m_axis_data_tvalid(fv),.m_axis_data_tready(1'b1),.m_axis_data_tlast(fl),
        .m_axis_status_tdata(status),.m_axis_status_tvalid(status_valid),.m_axis_status_tready(1'b1),
        .event_frame_started(efs),.event_tlast_unexpected(etlu),.event_tlast_missing(etlm),.event_fft_overflow(eov),
        .event_status_channel_halt(esh),.event_data_in_channel_halt(eih),.event_data_out_channel_halt(eoh));
    sa_fft_power power_dut(.clk(clk),.rst_n(rst_n),.log_n(logn[4:0]),.in_valid(fv),.in_data(fx),.in_last(fl),
        .in_index(fu[15:0]),.in_overflow(fu[16]),.out_valid(pv),.out_power(power),.out_last(pl),.out_index(pi),
        .quality_sticky(quality),.completed_frames(frames));
    initial begin
        if(!$value$plusargs("LOGN%d",logn))logn=7;
        n=1<<logn;cfg={7'd0,schedule(logn),1'b1,3'd0,logn[4:0]};
        $swrite(filename,"fft%0d_i.mem",logn); $readmemh(filename,vi,0,n-1);
        $swrite(filename,"fft%0d_q.mem",logn); $readmemh(filename,vq,0,n-1);
        $swrite(filename,"fft%0d_capture.csv",logn); fd=$fopen(filename,"w");
        $fdisplay(fd,"index,complex_hex,power_hex,last");
        repeat(12)@(negedge clk);rst_n=1;cfgv=1;
        @(posedge clk);while(!cfgr)@(posedge clk);
        @(negedge clk);cfgv=0;
        repeat(8)@(negedge clk);
        while(!fr)@(negedge clk);
        for(sent=0;sent<n;sent=sent+1)begin
            @(negedge clk);iv=1;i=vi[sent];q=vq[sent];
            @(negedge clk);iv=0;
        end
        wait(received==n);
        repeat(20)@(negedge clk);
        if(loss||lost||quality||frames!=1)begin $display("ERROR: FFT quality/loss/frame error"); $finish; end
        $fclose(fd);$display("SA_FFT_CAPTURE_COMPLETE N=%0d; numeric comparison still required",n);$finish;
    end
    always@(posedge clk)begin
        if(rst_n && (etlu||etlm||eov||esh||eoh))
            begin $display("ERROR: FFT event fault tlast_unexpected=%b missing=%b overflow=%b status_halt=%b out_halt=%b",etlu,etlm,eov,esh,eoh); $finish; end
        if(fv)raw[fu[15:0]]<=fx;
        #1;
        if(pv)begin
            if(pi!=received || pl!=(received==n-1))begin $display("ERROR: FFT output framing/index error"); $finish; end
            $fdisplay(fd,"%0d,%012h,%016h,%0d",pi,raw[pi],power,pl);
            received=received+1;
        end
    end
    initial begin #10000000;begin $display("ERROR: FFT timeout"); $finish; end end
endmodule
