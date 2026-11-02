`timescale 1ns/1ps
module tb_sa_result_control;
    reg ac=0,dc=0;always #5 ac=~ac;always #1.667 dc=~dc;
    reg resetn=0;reg [11:0] aw=0,ar=0;reg av=0,wv=0,bready=0,arv=0,rr=0;
    reg [31:0] wd=0;reg [3:0] ws=0;wire awr,wr,bv,arr,rv;wire [1:0] br,rresp;wire [31:0] rd;
    wire pipe,stream_reset;wire [63:0] data;wire [7:0] keep;wire valid,last;reg ready=1;
    integer release_edges=0;
    reg reset_checks_armed=0;
    reg pv=0,pl=0;reg [63:0] pd=0,fc=0,counter=0;reg [15:0] ix=0;
    wire [4:0] live;reg [31:0] got;integer packets=0,beat=0,k,code;reg [31:0] words[0:15];
    sa_result_bd #(.MAX_LOG_N(7)) dut(.ctrl_clk(ac),.ctrl_resetn(resetn),.dsp_clk(dc),.dsp_resetn(resetn),
      .s_axi_awaddr(aw),.s_axi_awprot(3'd0),.s_axi_awvalid(av),.s_axi_awready(awr),
      .s_axi_wdata(wd),.s_axi_wstrb(ws),.s_axi_wvalid(wv),.s_axi_wready(wr),
      .s_axi_bresp(br),.s_axi_bvalid(bv),.s_axi_bready(bready),
      .s_axi_araddr(ar),.s_axi_arprot(3'd0),.s_axi_arvalid(arv),.s_axi_arready(arr),
      .s_axi_rdata(rd),.s_axi_rresp(rresp),.s_axi_rvalid(rv),.s_axi_rready(rr),
      .m_axis_tdata(data),.m_axis_tkeep(keep),.m_axis_tvalid(valid),.m_axis_tlast(last),.m_axis_tready(ready),
      .pipe_resetn(pipe),.stream_resetn(stream_reset),.config_update(1'b0),.power_valid(pv),.power_data(pd),.power_index(ix),.power_last(pl),
      .quality(32'd0),.frame_count(fc),.sample_counter(counter),.dsp_status(32'h0000e00f),
      .log_n(5'd7),.right_shift(5'd8),.live_status(live));
    always @(posedge dc)begin
      if(!dut.parent_resetn)release_edges=0;else release_edges=release_edges+1;
      counter<=counter+4;
      if(valid&&ready)begin
        if(keep!=255)begin $display("ERROR: assertion failed");$display("TKEEP");$finish;end 
        if(beat==5 && data[31:0]!=(packets==0?101:103))begin $display("ERROR: assertion failed");$display("ticket atomicity or stale packet");$finish;end 
        if(last)begin if(beat!=159)begin $display("ERROR: assertion failed");$display("packet length");$finish;end packets=packets+1;beat=0;end
        else beat=beat+1;
      end
    end
    always @(negedge dut.parent_resetn)begin
      #0.001;
      if(reset_checks_armed && stream_reset!==1'b0)begin $display("ERROR: stream reset assertion is not asynchronous");$finish;end
    end
    always @(posedge stream_reset)begin
      if(reset_checks_armed && (!dut.parent_resetn || release_edges<3))begin $display("ERROR: stream reset released before three DSP clock edges");$finish;end
    end
    // XPM INIT_SYNC_FF models the FPGA global start-up reset. Its time-zero
    // initialization is not a runtime reset release. Hold the external reset
    // beyond GSR, then check both the initial assertion and every later edge.
    initial begin
      wait(glbl.GSR===1'b0); #0.01;
      if(stream_reset!==1'b0)begin $display("ERROR: stream reset not asserted after FPGA global reset");$finish;end
      reset_checks_armed=1;
    end
    task write_reg;
      input [11:0] addr;input [31:0] value;input [3:0] mask;input integer order,expected;
      reg [1:0] saved;begin
        @(negedge ac);aw=addr;wd=value;ws=mask;
        if(order==0)begin av=1;while(!awr)@(negedge ac);@(negedge ac);av=0;repeat(3)@(negedge ac);wv=1;while(!wr)@(negedge ac);@(negedge ac);wv=0;end
        else begin wv=1;while(!wr)@(negedge ac);@(negedge ac);wv=0;repeat(2)@(negedge ac);av=1;while(!awr)@(negedge ac);@(negedge ac);av=0;end
        while(!bv)@(negedge ac);saved=br;
        repeat(3)begin @(negedge ac);if(!bv||br!==saved)begin $display("ERROR: assertion failed");$display("B unstable");$finish;end end
        if(br!=expected)begin $display("ERROR: assertion failed");$display("BRESP addr=%h actual=%d expected=%d",addr,br,expected);$finish;end 
        bready=1;@(negedge ac);bready=0;
      end
    endtask
    task read_reg;
      input [11:0] addr;input integer expected;
      reg [31:0] saved;begin
        @(negedge ac);ar=addr;arv=1;while(!arr)@(negedge ac);@(negedge ac);arv=0;
        while(!rv)@(negedge ac);saved=rd;
        repeat(3)begin @(negedge ac);if(!rv||rd!==saved)begin $display("ERROR: assertion failed");$display("R unstable");$finish;end end
        if(rresp!=expected)begin $display("ERROR: assertion failed");$display("RRESP");$finish;end got=rd;rr=1;@(negedge ac);rr=0;
      end
    endtask
    task ticket;
      input integer id;
      begin words[3]=id;for(k=0;k<16;k=k+1)write_reg(12'h100+4*k,words[k],15,k%2,0);end
    endtask
    task frame;
      input integer id;integer j;
      begin
        repeat(80)@(negedge dc);
        for(j=0;j<128;j=j+1)begin
          @(negedge dc);pv=1;ix=j;pl=(j==127);pd=10000+j+id;fc=(j==127)?6:5;
          @(negedge dc);pv=0;pl=0;
        end
      end
    endtask
    task reply;
      input integer expected,id;integer tries;
      begin
        got=0;tries=0;
        while(!got[2]&&tries<100)begin read_reg(4,0);tries=tries+1;end
        if(!got[2]||got[1])begin $display("ERROR: assertion failed");$display("completion timeout");$finish;end 
        read_reg(12'hc,0);if(got!=expected)begin $display("ERROR: assertion failed");$display("response code %d",got);$finish;end 
        read_reg(12'h10,0);if(got!=id)begin $display("ERROR: assertion failed");$display("response ticket");$finish;end 
      end
    endtask
    initial begin
      for(k=0;k<16;k=k+1)words[k]=0;
      words[0]=23;words[1]=2;words[2]=1;words[4]=1800000000;words[7]=32'h4000;
      words[8]=99;words[10]=32'd2400000000;words[11]=1200000000;words[12]=1;words[13]=32'h701;words[15]=32'h52585331;
      repeat(20)@(negedge ac);resetn=1;repeat(20)@(negedge ac);
      read_reg(0,0);if(got!=1||pipe)begin $display("ERROR: assertion failed");$display("must start in HOLD_RESET");$finish;end 
      read_reg(8,0);if(got!=32'h31535852)begin $display("ERROR: assertion failed");$display("magic");$finish;end 
      write_reg(0,256,15,0,2);
      write_reg(12'h103,0,15,1,2);read_reg(12'h44,2);
      write_reg(12'h100,32'h12345678,15,0,0);write_reg(12'h100,32'haabbccdd,5,1,0);
      read_reg(12'h100,0);if(got!=32'h12bb56dd)begin $display("ERROR: assertion failed");$display("WSTRB");$finish;end 
      write_reg(0,0,15,1,0);repeat(30)@(negedge ac);
      ticket(101);write_reg(0,256,15,0,0);
      write_reg(0,256,15,1,2);write_reg(12'h10c,999,15,0,2);
      frame(101);reply(1,101);wait(packets==1);
      repeat(30)@(negedge ac);
      ticket(102);write_reg(0,256,15,1,0);repeat(30)@(negedge ac);
      write_reg(0,2,1,0,0);reply(7,102);
      write_reg(0,1,15,1,0);repeat(150)@(negedge ac);
      if(valid||live!=0)begin $display("ERROR: assertion failed");$display("flush leak");$finish;end 
      write_reg(0,0,15,0,0);repeat(150)@(negedge ac);
      read_reg(4,0);if(got[2:0]!=1)begin $display("ERROR: assertion failed");$display("stale response after flush");$finish;end 
      ticket(103);write_reg(0,256,15,1,0);frame(103);reply(1,103);wait(packets==2);
      repeat(100)@(negedge ac);$display("SA_RESULT_CONTROL_PASS packets=%d",packets);$finish;
    end
    initial begin #1000000;begin $display("ERROR: assertion failed");$display("test timeout");$finish;end end
endmodule
