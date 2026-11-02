`timescale 1ns/1ps
// Functional synchronizer shim for the unit test only. This is NOT CDC signoff.
module xpm_cdc_array_single #(parameter DEST_SYNC_FF=3,SRC_INPUT_REG=0,WIDTH=8)(
 input src_clk,input [WIDTH-1:0] src_in,input dest_clk,output [WIDTH-1:0] dest_out);
 reg [WIDTH-1:0] stage [0:DEST_SYNC_FF-1]; integer k,j;
 initial for(k=0;k<DEST_SYNC_FF;k=k+1) stage[k]=0;
 always @(posedge dest_clk) begin
  stage[0]<=src_in;
  for(j=1;j<DEST_SYNC_FF;j=j+1) stage[j]<=stage[j-1];
 end
 assign dest_out=stage[DEST_SYNC_FF-1];
endmodule
module tb_dac_stream_guard;
 reg clk=0,ctrl_clk=0,resetn=0;
 always #1.667 clk=~clk;
 always #5 ctrl_clk=~ctrl_clk;
 reg [31:0] control=0,fifo_words=0;
 reg [127:0] din=1;
 reg [15:0] keep=16'hffff;
 reg last=0,valid=1,ready=1;
 wire sready,mvalid; wire [127:0] dout;
 wire [31:0] status,underruns,frames;
 dac_stream_guard dut(.clk(clk),.ctrl_clk(ctrl_clk),.resetn(resetn),
   .control(control),.fifo_words(fifo_words),.ddr_calibrated(1'b1),
   .dac_clock_locked(1'b1),.status(status),.underrun_count(underruns),.frame_count(frames),
   .s_axis_tdata(din),.s_axis_tkeep(keep),.s_axis_tlast(last),.s_axis_tvalid(valid),
   .s_axis_tready(sready),.m_axis_tdata(dout),.m_axis_tvalid(mvalid),.m_axis_tready(ready));
 wire flush_resetn;
 tx_reset_sync flush(.clk(clk),.parent_resetn(resetn),.control(control),.resetn(flush_resetn));
 always @(posedge clk) if(resetn && valid && sready) din<=din+1;
 task tick; input integer n; repeat(n) @(negedge clk); endtask
 reg [127:0] held;
 initial begin
  tick(5); resetn=1; tick(8);
  if(dout!==0 || sready!==0 || mvalid!==1) begin $display("ERROR: idle is not muted"); $finish; end
  control=1; tick(8);
  if(sready!==0 || dout!==0) begin $display("ERROR: played before prefill"); $finish; end
  fifo_words=256; tick(8);
  if(!sready || dout==0 || underruns!=0) begin $display("ERROR: prefill did not start playback"); $finish; end
  ready=0; held=dout; control=0;
  repeat(6) begin tick(1); if(dout!==held) begin $display("ERROR: AXIS data changed under backpressure"); $finish; end end
  ready=1; tick(3);
  if(dout!==0 || sready) begin $display("ERROR: stop failed to mute"); $finish; end
  control=1; tick(8); valid=0; tick(7);
  if(underruns==0 || dout!==0) begin $display("ERROR: underflow did not insert zero/latch count"); $finish; end
  tick(15); if(!status[5]) begin $display("ERROR: underflow status not synchronized"); $finish; end
  valid=1; keep=16'h00ff; last=1; tick(20);
  if(!status[6] || frames==0) begin $display("ERROR: partial keep/frame checks failed"); $finish; end
  control=0; tick(20);
  if(underruns!==0 || frames!==0 || status[6:4]!==0) begin $display("ERROR: stop did not clear session flags"); $finish; end
  control=2; #0.1;
  if(flush_resetn!==0) begin $display("ERROR: flush reset failed asynchronous assertion"); $finish; end
  tick(2); control=0; tick(2);
  if(flush_resetn!==0) begin $display("ERROR: flush reset released too early"); $finish; end
  tick(3); if(flush_resetn!==1) begin $display("ERROR: flush reset did not release synchronously"); $finish; end
  $display("DAC_STREAM_GUARD_UNIT_PASS: mute/prefill/stall/underflow/keep/frame/flush");
  $finish;
 end
 initial begin #10000; $display("ERROR: unit test timeout"); $finish; end
endmodule
