`timescale 1ns/1ps
module sa_result_bd # (parameter integer MAX_LOG_N=16)(
    (* X_INTERFACE_PARAMETER="XIL_INTERFACENAME ctrl_clk, ASSOCIATED_BUSIF S_AXI, ASSOCIATED_RESET ctrl_resetn, FREQ_HZ 99999001" *)
    (* X_INTERFACE_INFO="xilinx.com:signal:clock:1.0 ctrl_clk CLK" *) input wire ctrl_clk,
    (* X_INTERFACE_PARAMETER="XIL_INTERFACENAME ctrl_resetn, POLARITY ACTIVE_LOW" *)
    (* X_INTERFACE_INFO="xilinx.com:signal:reset:1.0 ctrl_resetn RST" *) input wire ctrl_resetn,
    (* X_INTERFACE_PARAMETER="XIL_INTERFACENAME dsp_clk, ASSOCIATED_BUSIF M_AXIS, ASSOCIATED_RESET dsp_resetn, FREQ_HZ 300000000" *)
    (* X_INTERFACE_INFO="xilinx.com:signal:clock:1.0 dsp_clk CLK" *) input wire dsp_clk,
    (* X_INTERFACE_PARAMETER="XIL_INTERFACENAME dsp_resetn, POLARITY ACTIVE_LOW" *)
    (* X_INTERFACE_INFO="xilinx.com:signal:reset:1.0 dsp_resetn RST" *) input wire dsp_resetn,
    (* X_INTERFACE_PARAMETER="XIL_INTERFACENAME S_AXI, PROTOCOL AXI4LITE, DATA_WIDTH 32, ADDR_WIDTH 12" *)
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI AWADDR" *) input wire [11:0] s_axi_awaddr,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI AWPROT" *) input wire [2:0] s_axi_awprot,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI AWVALID" *) input wire s_axi_awvalid,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI AWREADY" *) output wire s_axi_awready,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI WDATA" *) input wire [31:0] s_axi_wdata,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI WSTRB" *) input wire [3:0] s_axi_wstrb,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI WVALID" *) input wire s_axi_wvalid,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI WREADY" *) output wire s_axi_wready,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI BRESP" *) output wire [1:0] s_axi_bresp,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI BVALID" *) output wire s_axi_bvalid,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI BREADY" *) input wire s_axi_bready,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI ARADDR" *) input wire [11:0] s_axi_araddr,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI ARPROT" *) input wire [2:0] s_axi_arprot,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI ARVALID" *) input wire s_axi_arvalid,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI ARREADY" *) output wire s_axi_arready,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI RDATA" *) output wire [31:0] s_axi_rdata,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI RRESP" *) output wire [1:0] s_axi_rresp,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI RVALID" *) output wire s_axi_rvalid,
    (* X_INTERFACE_INFO="xilinx.com:interface:aximm:1.0 S_AXI RREADY" *) input wire s_axi_rready,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TDATA" *) output wire [63:0] m_axis_tdata,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TKEEP" *) output wire [7:0] m_axis_tkeep,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TVALID" *) output wire m_axis_tvalid,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TREADY" *) input wire m_axis_tready,
    (* X_INTERFACE_INFO="xilinx.com:interface:axis:1.0 M_AXIS TLAST" *) output wire m_axis_tlast,
    output wire pipe_resetn,output wire stream_resetn,
    input wire config_update,input wire power_valid,input wire [63:0] power_data,
    input wire [15:0] power_index,input wire power_last,input wire [31:0] quality,
    input wire [63:0] frame_count,input wire [63:0] sample_counter,input wire [31:0] dsp_status,
    input wire [4:0] log_n,input wire [4:0] right_shift,output wire [4:0] live_status
);
    wire parent_resetn=dsp_resetn && pipe_resetn;
    wire local_resetn;
    // Vendor reset synchronizer includes the scoped CDC attributes/constraints
    // for asynchronous assertion and three-stage DSP-clock synchronous release.
    xpm_cdc_async_rst #(.DEST_SYNC_FF(3),.INIT_SYNC_FF(1),.RST_ACTIVE_HIGH(0)) reset_cdc(
        .src_arst(parent_resetn),.dest_clk(dsp_clk),.dest_arst(local_resetn));
    assign stream_resetn=local_resetn;
    wire reqvalid,reqready,rspvalid,rspready,cancel;wire [511:0] reqmeta;wire [191:0] rspdata;
    sa_result_control control(.aclk(ctrl_clk),.aresetn(ctrl_resetn),.dsp_clk(dsp_clk),.dsp_resetn(local_resetn),
        .awaddr(s_axi_awaddr),.awvalid(s_axi_awvalid),.awready(s_axi_awready),
        .wdata(s_axi_wdata),.wstrb(s_axi_wstrb),.wvalid(s_axi_wvalid),.wready(s_axi_wready),
        .bresp(s_axi_bresp),.bvalid(s_axi_bvalid),.bready(s_axi_bready),
        .araddr(s_axi_araddr),.arvalid(s_axi_arvalid),.arready(s_axi_arready),
        .rdata(s_axi_rdata),.rresp(s_axi_rresp),.rvalid(s_axi_rvalid),.rready(s_axi_rready),
        .pipe_resetn(pipe_resetn),.dsp_cancel(cancel),.command_valid(reqvalid),.command_meta(reqmeta),.command_ready(reqready),
        .response_valid(rspvalid),.response_data(rspdata),.response_ready(rspready),.live_status(live_status));
    sa_snapshot #(.MAX_LOG_N(MAX_LOG_N)) snapshot(.clk(dsp_clk),.rst_n(local_resetn),
        .cancel(cancel),.config_update(config_update),.request_valid(reqvalid),.request_meta(reqmeta),.request_ready(reqready),
        .response_valid(rspvalid),.response_data(rspdata),.response_ready(rspready),
        .power_valid(power_valid),.power_data(power_data),.power_index(power_index),.power_last(power_last),
        .quality(quality),.frame_count(frame_count),.sample_counter(sample_counter),.dsp_status(dsp_status),
        .log_n(log_n),.right_shift(right_shift),.m_axis_tdata(m_axis_tdata),.m_axis_tkeep(m_axis_tkeep),
        .m_axis_tvalid(m_axis_tvalid),.m_axis_tlast(m_axis_tlast),.m_axis_tready(m_axis_tready),.live_status(live_status));
endmodule
