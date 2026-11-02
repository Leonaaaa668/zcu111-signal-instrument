`timescale 1ns/1ps
// Hold AXIS stable under backpressure. An unstoppable-source collision is
// explicitly counted; the whole measurement epoch must then be discarded.
module sa_axis_loss_guard #(parameter WIDTH=48)(
    input wire clk,input wire rst_n,
    input wire source_valid,input wire [WIDTH-1:0] source_data,input wire source_last,
    output reg m_axis_tvalid,output reg [WIDTH-1:0] m_axis_tdata,output reg m_axis_tlast,
    input wire m_axis_tready,
    output reg loss_sticky,output reg [31:0] lost_samples
);
    always @(posedge clk) begin
        if(!rst_n) begin m_axis_tvalid<=0;m_axis_tdata<=0;m_axis_tlast<=0;loss_sticky<=0;lost_samples<=0;end
        else if(!m_axis_tvalid || m_axis_tready) begin
            m_axis_tvalid<=source_valid;
            if(source_valid) begin m_axis_tdata<=source_data;m_axis_tlast<=source_last;end
        end else if(source_valid) begin
            loss_sticky<=1;
            if(lost_samples!=32'hffffffff) lost_samples<=lost_samples+1;
        end
    end
endmodule
