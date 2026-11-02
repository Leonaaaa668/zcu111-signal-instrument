`timescale 1ns/1ps
// A fixed-rate RF-DAC cannot tolerate a software/DMA pause. Send zeros when
// stopped; latch/count underflow when running. No RF-power calibration implied.
module dac_stream_guard #(
    parameter integer PREFILL_WORDS = 256
)(
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS, ASSOCIATED_RESET resetn, FREQ_HZ 300000000" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 clk CLK" *) input wire clk,
    (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 resetn RST" *) input wire resetn,
    input wire ctrl_clk,
    input wire [31:0] control,
    input wire [31:0] fifo_words,
    input wire ddr_calibrated,
    input wire dac_clock_locked,
    output wire [31:0] status,
    output reg [31:0] underrun_count,
    output reg [31:0] frame_count,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDATA" *) input wire [127:0] s_axis_tdata,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TKEEP" *) input wire [15:0] s_axis_tkeep,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TLAST" *) input wire s_axis_tlast,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TVALID" *) input wire s_axis_tvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TREADY" *) output wire s_axis_tready,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDATA" *) output wire [127:0] m_axis_tdata,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TVALID" *) output wire m_axis_tvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *) input wire m_axis_tready
);
    (* ASYNC_REG = "TRUE" *) reg [1:0] enable_sync = 0;
    reg started, bad_keep, underrun_seen, playing_report;
    reg [127:0] output_data;
    wire enabled = enable_sync[1];
    wire playing = enabled && started;
    // Source-register diagnostic bits before the PS-domain synchronizer.
    // Do not send a binary-counter OR tree or an FSM combinational expression
    // across clocks: even independent status bits should be glitch-free.
    always @(posedge clk) begin
        if (!resetn) playing_report <= 0;
        else playing_report <= playing;
    end
    always @(posedge clk) begin
        if (!resetn) output_data <= 0;
        else if (m_axis_tready)
            output_data <= (playing && s_axis_tvalid) ? s_axis_tdata : 128'b0;
    end
    always @(posedge clk) begin
        if (!resetn) enable_sync <= 0;
        else enable_sync <= {enable_sync[0],control[0]};
    end
    always @(posedge clk) begin
        if (!resetn || !enabled) begin
            started <= 0;
            underrun_count <= 0;
            frame_count <= 0;
            bad_keep <= 0;
            underrun_seen <= 0;
        end else begin
            if (!started && fifo_words >= PREFILL_WORDS) started <= 1;
            if (playing && m_axis_tready && !s_axis_tvalid) begin
                underrun_seen <= 1;
                if (underrun_count != 32'hffffffff)
                    underrun_count <= underrun_count + 1'b1;
            end
            if (s_axis_tready && s_axis_tvalid) begin
                if (s_axis_tlast) frame_count <= frame_count + 1'b1;
                if (s_axis_tkeep != 16'hffff) bad_keep <= 1;
            end
        end
    end
    assign s_axis_tready = playing && m_axis_tready;
    assign m_axis_tvalid = resetn;
    assign m_axis_tdata = output_data;
    // Only independent/sticky bits cross here, not the binary counters.
    wire [7:0] flags = {m_axis_tready,bad_keep,underrun_seen,playing_report,
                         s_axis_tvalid,dac_clock_locked,ddr_calibrated,resetn};
    wire [7:0] synchronized_flags;
    xpm_cdc_array_single #(.DEST_SYNC_FF(3),.SRC_INPUT_REG(0),.WIDTH(8)) status_cdc (
        .src_clk(clk),.src_in(flags),.dest_clk(ctrl_clk),.dest_out(synchronized_flags));
    assign status = {24'b0,synchronized_flags};
endmodule
