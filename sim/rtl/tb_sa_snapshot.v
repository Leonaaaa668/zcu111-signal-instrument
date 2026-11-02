`timescale 1ns/1ps
module tb_sa_snapshot;
    reg clk=0;always #5 clk=~clk;
    reg rst_n=0,cancel=0,config_update=0,req=0;
    reg [511:0] meta=0;wire reqready,rspvalid;wire [191:0] rsp;
    reg pv=0,pl=0;reg [63:0] pd=0,frames=0,counter=0;
    reg [15:0] index=0;reg [31:0] quality=0,status=0;
    reg [4:0] logn=7,shift=8;
    wire [63:0] data;wire [7:0] keep;wire valid,last;reg ready=0;
    wire [4:0] live;
    sa_snapshot dut(.clk(clk),.rst_n(rst_n),.cancel(cancel),.config_update(config_update),
        .request_valid(req),.request_meta(meta),.request_ready(reqready),
        .response_valid(rspvalid),.response_data(rsp),.response_ready(1'b1),
        .power_valid(pv),.power_data(pd),.power_index(index),.power_last(pl),.quality(quality),
        .frame_count(frames),.sample_counter(counter),.dsp_status(status),.log_n(logn),.right_shift(shift),
        .m_axis_tdata(data),.m_axis_tkeep(keep),.m_axis_tvalid(valid),.m_axis_tlast(last),
        .m_axis_tready(ready),.live_status(live));
    integer fd,packet=0,beat=0,responses=0,expected_response=0,lastcode=0,stimulus=0;
    integer saved_count,j;
    reg blocked=0,held=0;
    reg [31:0] random_state=32'h8461beef;
    reg [63:0] held_data;reg held_last;
    always @(negedge clk)begin
        random_state={random_state[30:0],random_state[31]^random_state[21]^random_state[1]^random_state[0]};
        ready=rst_n && !blocked && (random_state[2:0]!=0);
    end
    always @(posedge clk)begin
        if(!rst_n)begin counter<=0;held<=0;end
        else begin
            counter<=counter+4;
            if(held && (!valid || data!==held_data || last!==held_last || keep!=8'hff))begin
                $display("ERROR: AXIS changed under backpressure");$finish;
            end
            held<=valid&&!ready;held_data<=data;held_last<=last;
            if(rspvalid)begin responses=responses+1;lastcode=rsp[31:0];end
            if(valid && ready)begin
                if(keep!=8'hff)begin $display("ERROR: TKEEP");$finish;end
                $fdisplay(fd,"%0d,%0d,%016h,%0d",packet,beat,data,last);
                if(last)begin packet=packet+1;beat=0;end else beat=beat+1;
            end
        end
    end
    task issue;
        input integer id,ln,badmeta;
        begin
            @(negedge clk);logn=ln;shift=ln+1;status=(ln<<13)|15;stimulus=id;
            meta=0;meta[31:0]=23;meta[63:32]=2;meta[95:64]=1;meta[127:96]=id;
            meta[191:128]=64'd1800000000;meta[255:192]=64'h400000000000;
            meta[319:256]=64'h123456789;meta[351:320]=32'd2400000000;
            meta[383:352]=32'd1200000000;meta[415:384]=1;
            meta[447:416]=(ln<<8)|1;meta[511:480]=badmeta?0:32'h52585331;
            expected_response=responses+1;
            while(!reqready)@(negedge clk);
            req=1;@(negedge clk);req=0;
        end
    endtask
    task check_reply;
        input integer code;
        begin
            while(responses<expected_response)@(negedge clk);
            if(responses!=expected_response || lastcode!=code)begin
                $display("ERROR: response expected=%0d actual=%0d count=%0d/%0d",code,lastcode,responses,expected_response);$finish;
            end
        end
    endtask
    task send_frame;
        input integer id,ln,fault;
        integer k,n;
        begin
            n=1<<ln;
            for(k=0;k<n;k=k+1)begin
                @(negedge clk);pv=1;index=k;
                if(k==0)pd=64'h0000800000000000;
                else if(k==1)pd=0;
                else pd=64'h0000400000000000+64'h0000000100000000*(k&16383)+64'd33*k+id;
                pl=(k==n-1);frames=1000+2*id+((k==n-1)?1:0);
                if(fault==1 && k==7)index=8;
                if(fault==2 && k==9)pl=1;
                if(fault==3 && k==n-1)pl=0;
                if(fault==5 && k==10)config_update=1;
                if(fault==6 && k==10)cancel=1;
                if(fault==7 && k==7)pd=64'hffff000000000000;
                if(fault==8 && k==7)frames=frames+1;
                if(fault==9 && k==n-1)frames=frames-1;
                @(negedge clk);pv=0;pl=0;config_update=0;
                if(k%5==0)@(negedge clk);
            end
            if(fault==4)begin repeat(2)@(negedge clk);quality=1;end
        end
    endtask
    initial begin
        fd=$fopen("packets.csv","w");$fdisplay(fd,"packet,beat,data_hex,last");
        repeat(8)@(negedge clk);rst_n=1;
        issue(1,7,0);send_frame(1,7,0);check_reply(1);wait(packet==1);
        issue(2,10,0);send_frame(2,10,0);check_reply(1);wait(packet==2);
        issue(3,16,0);send_frame(3,16,0);check_reply(1);wait(packet==3);
        @(negedge clk);blocked=1;
        issue(4,7,0);send_frame(4,7,0);check_reply(1);
        issue(5,7,0);send_frame(5,7,0);check_reply(1);
        issue(6,7,0);check_reply(2);
        if(live[1:0]!=3)begin $display("ERROR: dual banks not owned");$finish;end
        repeat(30)@(negedge clk);blocked=0;wait(packet==5);
        for(j=1;j<=7;j=j+1)begin
            issue(6+j,7,0);send_frame(6+j,7,j);check_reply((j==4)?5:((j==5)?4:((j==6)?7:6)));
            @(negedge clk);quality=0;cancel=0;
        end
        // Reject a frame counter that advances in the body or fails to
        // advance on the last bin, including after precomputed comparisons.
        for(j=8;j<=9;j=j+1)begin
            issue(30+j,7,0);send_frame(30+j,7,j);check_reply(6);
        end
        issue(14,7,1);check_reply(4);
        issue(15,7,0);repeat(5)@(negedge clk);cancel=1;check_reply(7);
        @(negedge clk);cancel=0;
        repeat(40)@(negedge clk);
        if(packet!=5 || beat!=0)begin $display("ERROR: bad frame produced a partial packet");$finish;end
        blocked=1;issue(16,7,0);send_frame(16,7,0);check_reply(1);
        wait(valid);@(negedge clk);rst_n=0;repeat(8)@(negedge clk);
        if(valid || live[1:0]!=0)begin $display("ERROR: reset leaked old packet");$finish;end
        rst_n=1;blocked=0;
        issue(20,7,0);send_frame(20,7,0);check_reply(1);wait(packet==6);
        repeat(20)@(negedge clk);$fclose(fd);
        $display("SA_SNAPSHOT_SIM_PASS packets=%0d; run independent packet decoder/payload check",packet);$finish;
    end
    initial begin #50000000;$display("ERROR: snapshot timeout");$finish;end
endmodule
