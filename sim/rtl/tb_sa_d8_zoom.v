`timescale 1ns/1ps
module tb_sa_d8_zoom;
    reg clk=0,rst_n=0,iv=0,qv=0;
    reg [63:0]i=0,q=0;
    reg [1023:0] filename;
    integer zoom=0,samples,beats,expected,received=0,sent=0,cycle=0,mode=0;
    reg [63:0]vi[0:16383],vq[0:16383];
    reg [23:0]ei[0:8191],eq[0:8191];
    wire signed[23:0]oi,oq;wire ov;
    wire [31:0]quality;wire[63:0]counter;
    always #5 clk=~clk;
    sa_d8_zoom dut(.clk(clk),.rst_n(rst_n),.adc_i(i),.adc_q(q),.adc_i_valid(iv),.adc_q_valid(qv),
        .monitor_enable(1'b1),.zoom_stages(zoom[2:0]),.out_i(oi),.out_q(oq),.out_valid(ov),
        .quality_sticky(quality),.adc_sample_counter(counter));
    initial begin
        if(!$value$plusargs("ZOOM%d",zoom))zoom=0;
        if(!$value$plusargs("MODE%d",mode))mode=0;
        samples=zoom==0 ? 8192:(zoom==2 ? 16384:65536);
        beats=samples/4;expected=samples/(8*(1<<zoom));
        $swrite(filename,"d8_z%0d_i.mem",zoom); $readmemh(filename,vi,0,beats-1);
        $swrite(filename,"d8_z%0d_q.mem",zoom); $readmemh(filename,vq,0,beats-1);
        $swrite(filename,"d8_z%0d_ei.mem",zoom); $readmemh(filename,ei,0,expected-1);
        $swrite(filename,"d8_z%0d_eq.mem",zoom); $readmemh(filename,eq,0,expected-1);
        repeat(5)@(negedge clk);rst_n=1;
        for(sent=0;sent<beats;sent=sent+1)begin
            @(negedge clk);i=vi[sent];q=vq[sent];iv=1;qv=1;
            if(mode==1 && sent==17)begin iv=0;qv=0;end
        end
        @(negedge clk);iv=0;qv=0;
        repeat(200)@(negedge clk);
        if(mode==0 && received!=expected)begin $display("ERROR: wrong count z=%d %d/%d",zoom,received,expected); $finish; end
        // After input ends, monitor sees a real gap: quality bit0 must latch.
        if(!quality[0])begin $display("ERROR: missing gap quality flag"); $finish; end
        $display("SA_D8_ZOOM_PASS zoom=%0d mode=%0d received=%0d",zoom,mode,received);$finish;
    end
    always@(posedge clk)begin
        #1;
        if(ov && mode==0)begin
            if(received>=expected || oi!==ei[received] || oq!==eq[received])
                begin $display("ERROR: D8 integer mismatch z=%d n=%d %h/%h %h/%h",zoom,received,oi,ei[received],oq,eq[received]); $finish; end
            received=received+1;
        end
    end
    initial begin #1000000;begin $display("ERROR: timeout"); $finish; end end
endmodule
