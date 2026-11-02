// Generated from coeff/sa/hann_metrics.json and the exact Hann MEM file.
function [63:0] window_sum;
    input [4:0] logn;
    begin case(logn)
        5'd7: window_sum=64'd8388607;
        5'd8: window_sum=64'd16777215;
        5'd9: window_sum=64'd33554431;
        5'd10: window_sum=64'd67108863;
        5'd11: window_sum=64'd134217725;
        5'd12: window_sum=64'd268435451;
        5'd13: window_sum=64'd536870901;
        5'd14: window_sum=64'd1073741803;
        5'd15: window_sum=64'd2147483607;
        5'd16: window_sum=64'd4294967215;
        default: window_sum=0;
    endcase end
endfunction
function [63:0] window_sum_square;
    input [4:0] logn;
    begin case(logn)
        5'd7: window_sum_square=64'd824633364049;
        5'd8: window_sum_square=64'd1649266376665;
        5'd9: window_sum_square=64'd3298534262833;
        5'd10: window_sum_square=64'd6597070144921;
        5'd11: window_sum_square=64'd13194139149795;
        5'd12: window_sum_square=64'd26388278898841;
        5'd13: window_sum_square=64'd52776554043067;
        5'd14: window_sum_square=64'd105553114496197;
        5'd15: window_sum_square=64'd211106226663861;
        5'd16: window_sum_square=64'd422212465703465;
        default: window_sum_square=0;
    endcase end
endfunction
localparam [255:0] WINDOW_MEM_SHA256=256'h8cbf9904c4f36ed5371f94e1d60f70c6703db1651d7b1a36021e85ae91a5e289;
