`timescale 1ns/1ps
// Sequential-sample 4->2, 2->1 or valid-gated 1->1/D2 complex FIR.
// Integrated by sa_d8_zoom as RX D8 stages d0/d1/d2 in the accepted design.
// Coefficients: signed18 Q1.17. Arithmetic: convergent rounding + saturation.
module sa_halfband_decim2 #(
    parameter integer IN_LANES = 4,
    parameter integer IN_WIDTH = 16,
    parameter integer OUT_WIDTH = 24,
    parameter integer TAPS = 15,
    parameter integer SHIFT = 9,
    parameter COEFF_FILE = "d2_1200_to600.mem",
    parameter integer OUT_LANES = (IN_LANES == 1 ? 1 : IN_LANES/2),
    parameter integer LEVELS = $clog2(TAPS),
    parameter integer PAD = 1 << LEVELS,
    parameter integer ACC_WIDTH = IN_WIDTH + 18 + LEVELS + 1
) (
    input wire clk,
    input wire rst_n,
    input wire in_valid,
    input wire [IN_LANES*IN_WIDTH-1:0] in_i,
    input wire [IN_LANES*IN_WIDTH-1:0] in_q,
    output reg out_valid,
    output reg [OUT_LANES*OUT_WIDTH-1:0] out_i,
    output reg [OUT_LANES*OUT_WIDTH-1:0] out_q,
    output reg out_saturated
);
    reg signed [17:0] coeff [0:TAPS-1];
    reg signed [IN_WIDTH-1:0] history_i [0:TAPS-2];
    reg signed [IN_WIDTH-1:0] history_q [0:TAPS-2];
    reg [LEVELS:0] valid_pipe;
    reg phase;
    wire accept_output = in_valid && (IN_LANES != 1 || !phase);
    wire [OUT_LANES-1:0] saturated_lanes;
    integer h;
    initial begin
        if ((IN_LANES != 1 && IN_LANES != 2 && IN_LANES != 4) ||
            TAPS < 7 || TAPS % 4 != 3 || SHIFT < 0 || SHIFT >= ACC_WIDTH || ACC_WIDTH > 63)
            begin $display("ERROR: Invalid FIR parameters"); $finish; end
        $readmemh(COEFF_FILE, coeff);
    end

    // Every lane is the next instant of the SAME complex signal.
    always @(posedge clk) begin
        if (!rst_n) begin
            phase <= 1'b0;
            valid_pipe <= 0;
            out_valid <= 0;
            out_saturated <= 0;
            for (h=0; h<TAPS-1; h=h+1) begin
                history_i[h] <= 0;
                history_q[h] <= 0;
            end
        end else begin
            if (in_valid) begin
                if (IN_LANES == 1) phase <= !phase;
                for (h=0; h<TAPS-1; h=h+1) begin
                    if (h < IN_LANES) begin
                        history_i[h] <= $signed(in_i[(IN_LANES-1-h)*IN_WIDTH +: IN_WIDTH]);
                        history_q[h] <= $signed(in_q[(IN_LANES-1-h)*IN_WIDTH +: IN_WIDTH]);
                    end else begin
                        history_i[h] <= history_i[h-IN_LANES];
                        history_q[h] <= history_q[h-IN_LANES];
                    end
                end
            end
            valid_pipe <= (valid_pipe << 1) | accept_output;
            out_valid <= valid_pipe[LEVELS];
            out_saturated <= valid_pipe[LEVELS] && (|saturated_lanes);
        end
    end

    function automatic [OUT_WIDTH:0] quantize;
        input signed [ACC_WIDTH-1:0] value;
        reg signed [ACC_WIDTH-1:0] base, remainder, half_value, rounded, minimum, maximum;
        begin
            base = value >>> SHIFT;
            rounded = base;
            if (SHIFT > 0) begin
                remainder = value - (base <<< SHIFT);
                half_value = 64'sd1 <<< (SHIFT-1);
                if (remainder > half_value || (remainder == half_value && base[0]))
                    rounded = base+1;
            end
            maximum = (64'sd1 <<< (OUT_WIDTH-1))-1;
            minimum = -(64'sd1 <<< (OUT_WIDTH-1));
            if (rounded > maximum) quantize = {1'b1, {1'b0, {(OUT_WIDTH-1){1'b1}}}};
            else if (rounded < minimum) quantize = {1'b1, {1'b1, {(OUT_WIDTH-1){1'b0}}}};
            else quantize = {1'b0, rounded[OUT_WIDTH-1:0]};
        end
    endfunction

    genvar lane, tap, level, node;
    generate for (lane=0; lane<OUT_LANES; lane=lane+1) begin: g_lane
        reg signed [ACC_WIDTH-1:0] tree_i [0:(LEVELS+1)*PAD-1];
        reg signed [ACC_WIDTH-1:0] tree_q [0:(LEVELS+1)*PAD-1];
        localparam integer POS = 2*lane;
        for (tap=0; tap<PAD; tap=tap+1) begin: g_mult
            if (tap < TAPS) begin: g_tap
                wire signed [IN_WIDTH-1:0] sample_i, sample_q;
                if (tap <= POS) begin: g_current
                    assign sample_i = $signed(in_i[(POS-tap)*IN_WIDTH +: IN_WIDTH]);
                    assign sample_q = $signed(in_q[(POS-tap)*IN_WIDTH +: IN_WIDTH]);
                end else begin: g_past
                    assign sample_i = history_i[tap-POS-1];
                    assign sample_q = history_q[tap-POS-1];
                end
                always @(posedge clk) begin
                    if (accept_output) begin
                        tree_i[(0)*PAD+(tap)] <= sample_i * coeff[tap];
                        tree_q[(0)*PAD+(tap)] <= sample_q * coeff[tap];
                    end
                end
            end else begin: g_pad
                always @(posedge clk) begin
                    tree_i[(0)*PAD+(tap)] <= 0;
                    tree_q[(0)*PAD+(tap)] <= 0;
                end
            end
        end
        for (level=1; level<=LEVELS; level=level+1) begin: g_level
            for (node=0; node<(PAD>>level); node=node+1) begin: g_sum
                always @(posedge clk) begin
                    tree_i[(level)*PAD+(node)] <= tree_i[(level-1)*PAD+(2*node)]+tree_i[(level-1)*PAD+(2*node+1)];
                    tree_q[(level)*PAD+(node)] <= tree_q[(level-1)*PAD+(2*node)]+tree_q[(level-1)*PAD+(2*node+1)];
                end
            end
        end
        wire [OUT_WIDTH:0] rounded_i = quantize(tree_i[(LEVELS)*PAD+(0)]);
        wire [OUT_WIDTH:0] rounded_q = quantize(tree_q[(LEVELS)*PAD+(0)]);
        assign saturated_lanes[lane] = rounded_i[OUT_WIDTH] | rounded_q[OUT_WIDTH];
        always @(posedge clk) begin
            if (!rst_n) begin
                out_i[lane*OUT_WIDTH +: OUT_WIDTH] <= 0;
                out_q[lane*OUT_WIDTH +: OUT_WIDTH] <= 0;
            end else if (valid_pipe[LEVELS]) begin
                out_i[lane*OUT_WIDTH +: OUT_WIDTH] <= rounded_i[OUT_WIDTH-1:0];
                out_q[lane*OUT_WIDTH +: OUT_WIDTH] <= rounded_q[OUT_WIDTH-1:0];
            end
        end
    end endgenerate
endmodule
