`timescale 1ns/1ps
// Whole custom frontend test. The external FFT handshake is controlled here;
// actual FFT arithmetic is independently covered by tb_sa_fft.v/vendor model.
module tb_sa_frontend_control;
    reg clk=0,rst_n=0,update=0,iv=1,qv=1,cfgr=0,ready=1,checking=0;
    always #1.667 clk=~clk;
    reg [31:0] cfg=32'h1000;
    wire frst,cv,av,al,pv,pl;
    wire [31:0] cd,quality,status;
    wire [47:0] ad;
    wire [63:0] power,frames,samples;
    wire [15:0] pi;
    wire [4:0] ln,shift;
    reg [17:0] hann[0:65535];
    integer seen=0,index,cycles;
    reg [47:0] held;
    sa_rx_dsp #(.MAX_ZOOM(0)) dut(.clk(clk),.rst_n(rst_n),.adc_i({4{16'sd3000}}),.adc_q({4{-16'sd1000}}),
        .adc_i_valid(iv),.adc_q_valid(qv),.config_word(cfg),.config_update(update),
        .fft_resetn(frst),.fft_config_data(cd),.fft_config_valid(cv),.fft_config_ready(cfgr),
        .fft_input_data(ad),.fft_input_valid(av),.fft_input_last(al),.fft_input_ready(ready),
        .fft_output_data(48'd0),.fft_output_valid(1'b0),.fft_output_last(1'b0),
        .fft_output_user(24'd0),.fft_events(6'd0),.power_valid(pv),.power_data(power),
        .power_last(pl),.power_index(pi),.quality(quality),.frame_count(frames),
        .sample_counter(samples),.status(status),.actual_log_n(ln),.fft_right_shift(shift));
    function [23:0] window_value;
        input signed [23:0] amplitude;input [17:0] coefficient;
        reg signed [63:0] product,base,remainder;
        begin
            product=amplitude*$signed(coefficient);
            base=product>>>17;remainder=product-(base<<<17);
            if(remainder>65536 || (remainder==65536 && base[0]))base=base+1;
            window_value=base[23:0];
        end
    endfunction
    task configure;
        input [31:0] word;
        begin
            checking=0;
            @(negedge clk);cfg=word;update=1;
            @(negedge clk);
            if(frst!==1'b0)begin $display("ERROR: configuration did not assert pipeline reset");$finish;end
            update=0;
            repeat(3)@(negedge clk);
            checking=1;
        end
    endtask
    task wait_frame;
        begin
            cycles=0;
            while(seen<(1<<ln) && cycles<100000)begin @(negedge clk);cycles=cycles+1;end
            if(seen<(1<<ln) || quality!=0 || status[3:0]!=15 || shift!=ln+1)begin
                $display("ERROR: frontend frame/quality/configuration %d %h %h",seen,quality,status);$finish;
            end
        end
    endtask
    always @(posedge clk)begin
        if(!frst)seen=0;
        else if(av && ready)begin
            index=seen% (1<<ln);
            if(checking && (ad!=={window_value(-24'sd256000,hann[index<<(16-ln)]),
                                  window_value(24'sd768000,hann[index<<(16-ln)])} || al!==(index==((1<<ln)-1))))begin
                $display("ERROR: frontend DC/Hann/order/TLAST n=%d data=%h",index,ad);$finish;
            end
            seen=seen+1;
        end
    end
    initial begin
        $readmemh("hann_q17.mem",hann);
        repeat(12)@(negedge clk);rst_n=1;
        configure(32'h80000701);
        wait(cv);held={16'd0,cd};
        repeat(8)begin @(negedge clk);if(!cv || cd!==held[31:0])begin
            $display("ERROR: configuration changed while stalled");$finish;end end
        cfgr=1;wait_frame;
        @(negedge clk);iv=0;
        @(negedge clk);iv=1;
        repeat(5)@(negedge clk);
        if(!quality[0] || !quality[1])begin $display("ERROR: ADC gap/IQ mismatch not marked");$finish;end
        configure(32'h00000801);wait_frame;
        checking=0;@(negedge clk);ready=0;
        wait(av);held=ad;
        repeat(40)begin @(negedge clk);if(ad!==held || !av)begin
            $display("ERROR: whole frontend AXIS changed during stall");$finish;end end
        if(!quality[16])begin $display("ERROR: whole frontend loss flag missing");$finish;end
        ready=1;
        configure(32'h80000701);wait_frame;
        configure(32'h00070701);repeat(60)@(negedge clk);
        if(frst || status[3])begin $display("ERROR: illegal zoom accepted");$finish;end
        configure(32'h00010701);repeat(60)@(negedge clk);
        if(frst || status[3])begin $display("ERROR: removed zoom1 accepted");$finish;end
        configure(32'h80000601);repeat(60)@(negedge clk);
        if(frst || status[3])begin $display("ERROR: illegal logN accepted");$finish;end
        configure(32'h00000a00);repeat(60)@(negedge clk);
        if(frst || av || quality!=0)begin $display("ERROR: RX stop did not reset/clear pipeline");$finish;end
        $display("SA_FRONTEND_CONTROL_PASS: settled D8/zoom DC+Hann/order/TLAST, config hold/restart, gap/mismatch, loss, invalid parameters, stop");
        $finish;
    end
    initial begin #2000000;$display("ERROR: frontend timeout");$finish;end
endmodule
