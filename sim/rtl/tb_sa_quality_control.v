`timescale 1ns/1ps
// Negative-path checks: AXIS stability/loss, FFT frame errors and atomic CDC.
module tb_sa_quality_control;
    reg clk=0,ctrl_clk=0,rst_n=0,ctrl_rst=0;
    always #1.667 clk=~clk;
    always #5 ctrl_clk=~ctrl_clk;
    reg sv=0,sl=0,ready=0;reg[47:0]sd=0;
    wire gv,gl,loss;wire[47:0]gd;wire[31:0]lost;
    sa_axis_loss_guard guard(.clk(clk),.rst_n(rst_n),.source_valid(sv),.source_data(sd),.source_last(sl),
        .m_axis_tvalid(gv),.m_axis_tdata(gd),.m_axis_tlast(gl),.m_axis_tready(ready),
        .loss_sticky(loss),.lost_samples(lost));
    reg pv=0,pl=0,overflow=0;reg[47:0]pd=0;reg[15:0]pi=0;
    wire ov,ol;wire[63:0]power,frames;wire[15:0]oi;wire[31:0]quality;
    sa_fft_power p(.clk(clk),.rst_n(rst_n),.log_n(5'd7),.in_valid(pv),.in_data(pd),
        .in_last(pl),.in_index(pi),.in_overflow(overflow),.out_valid(ov),.out_power(power),
        .out_last(ol),.out_index(oi),.quality_sticky(quality),.completed_frames(frames));
    reg[31:0]control=32'h1000,dsp_status=32'h00200015;
    wire[31:0]config_word,status;wire update;
    sa_rx_control_bd bridge(.ctrl_clk(ctrl_clk),.ctrl_resetn(ctrl_rst),.control(control),
        .dsp_clk(clk),.dsp_status(dsp_status),.status(status),.config_word(config_word),.config_update(update));
    integer updates=0;reg[31:0]last_config=0;
    always@(posedge clk)if(update)begin
        if(config_word!==32'h1000 && config_word!==32'h80000701 && config_word!==32'h00060f01)
            begin $display("ERROR: torn/unexpected CDC configuration %h",config_word); $finish; end
        updates=updates+1;last_config=config_word;
    end
    task wait_config;
        input [31:0] expected;
        integer timeout;begin
            timeout=0;
            while(last_config!==expected && timeout<200)begin @(negedge ctrl_clk);timeout=timeout+1;end
            if(last_config!==expected)begin $display("ERROR: configuration not applied %h",expected); $finish; end
            repeat(20)@(negedge ctrl_clk);
        end
    endtask
    initial begin
        repeat(8)@(negedge clk);rst_n=1;
        // Store one sample and hold it under backpressure. New data is lost,
        // while all AXIS fields of the stored transaction remain unchanged.
        sv=1;sd=48'h123456abcdef;sl=1;
        @(negedge clk);sv=0;
        repeat(3)begin @(negedge clk);if(!gv || gd!==48'h123456abcdef || !gl)begin $display("ERROR: unstable AXIS hold"); $finish; end end
        sv=1;sd=48'h1;sl=0;
        @(negedge clk);sd=48'h2;
        @(negedge clk);sv=0;
        if(!loss || lost!=2 || gd!==48'h123456abcdef || !gl)begin $display("ERROR: lost sample accounting"); $finish; end
        ready=1;@(negedge clk);
        if(gv)begin $display("ERROR: accepted transaction was not retired"); $finish; end
        // Largest negative signed24 components must square as positive exact
        // power, without a signed48 overflow in the sum.
        pv=1;pd={24'h800000,24'h800000};pi=0;
        @(negedge clk);pv=0;
        @(negedge clk);
        if(!ov || power!==64'h0000800000000000 || oi!=0 || quality!=0)begin $display("ERROR: signed square/pipeline"); $finish; end
        // Deliberately skip index1, assert TLAST early, and mark FFT overflow.
        pv=1;pi=2;pl=1;overflow=1;pd=0;
        @(negedge clk);pv=0;overflow=0;pl=0;
        repeat(3)@(negedge clk);
        if(quality[2:0]!==3'b111 || frames!=1)begin $display("ERROR: missing frame/overflow quality"); $finish; end
        rst_n=0;repeat(3)@(negedge clk);rst_n=1;
        if(quality!=0 || loss || lost!=0 || frames!=0)begin $display("ERROR: reset did not clear epoch"); $finish; end
        @(negedge ctrl_clk);ctrl_rst=1;
        wait_config(32'h1000);
        @(negedge ctrl_clk);control=32'h80000701;wait_config(32'h80000701);
        @(negedge ctrl_clk);control=32'h00060f01;wait_config(32'h00060f01);
        repeat(20)@(negedge ctrl_clk);
        if(updates!=3 || status!==dsp_status)begin $display("ERROR: CDC duplicate/lost update or status mismatch"); $finish; end
        $display("SA_QUALITY_CONTROL_PASS: backpressure stable, loss counted, frame faults, signed power, epoch reset, atomic CDC");
        $finish;
    end
    initial begin #100000;begin $display("ERROR: timeout"); $finish; end end
endmodule
