#include "core.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <chrono>
using namespace instrument;
namespace {
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void rejects(const std::function<void()> &test){try{test();}catch(const std::exception&){return;}throw std::runtime_error("Invalid data was accepted");}
}
int main(int argc,char **argv){
    try{
        check(argc==3,"Need resources and test-output root");const fs::path root=argv[1],base=argv[2];fs::create_directories(base);
        const auto out=base/("run_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(out),"Test output exists");auto resources=loadResources(root);unsigned tests=0;
        check(sha256(Bytes{})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 empty vector");
        check(sha256(Bytes{'a','b','c'})=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc vector");++tests;
        for(const auto &name:{"lower","n8192","n65536"}){
            auto file=root/"fixtures"/name/"spectrum.bin";auto receipt=loadReceipt(file.string()+".receipt.json");
            auto packet=decode(readFile(file),receipt,resources);auto m=measure({packet});
            check(m.n==(1u<<receipt.logn)&&m.frames==1,"FFT geometry");
            check(std::abs(m.spacing-150e6/m.n)<1e-10&&m.enbwBins>1.4&&m.enbwBins<1.6,"Resolution/window normalization");
            check(m.displayDbfs.size()==m.n,"Curve shape");saveMeasurement(out/(std::string(name)+"_measurement"),m);
            auto raw=readFile(file);raw.back()^=1;rejects([&]{decode(raw,receipt,resources);});++tests;
            raw=readFile(file);receipt.ticket++;rejects([&]{decode(raw,receipt,resources);});++tests;
            rejects([&]{measure({packet,packet});});++tests;
        }
        auto a=root/"fixtures/lower/spectrum.bin",b=root/"fixtures/upper/spectrum.bin";
        auto pa=decode(readFile(a),loadReceipt(a.string()+".receipt.json"),resources);
        auto pb=decode(readFile(b),loadReceipt(b.string()+".receipt.json"),resources);auto average=measure({pa,pb});
        check(average.frames==2,"Two independent frames");for(unsigned k=0;k<average.n;++k)check(average.average[k]==(double(pa.power[k])+double(pb.power[k]))/2,"Linear-power averaging");++tests;
        Receipt large=pa.receipt;large.sequence=18446744073709551615ULL;large.counterEpoch=9007199254740993ULL;
        auto json=receiptJson(large);writeFile(out/"large_receipt.json",Bytes(json.begin(),json.end()));const auto reread=loadReceipt(out/"large_receipt.json");
        check(reread.sequence==large.sequence&&reread.counterEpoch==large.counterEpoch,"64bit receipt precision");++tests;
        auto damaged=resources;damaged.fingerprint[0]^=1;rejects([&]{decode(readFile(a),pa.receipt,damaged);});++tests;
        auto unknown=out/"unknown_receipt.json";json="{\"ticket_id\":1,\"epoch\":0,\"segment_id\":0,\"nco_update_seq\":0,\"log_n\":10,\"zoom\":0,\"packet_sequence\":1,\"BAD_KEY\":0}";writeFile(unknown,Bytes(json.begin(),json.end()));rejects([&]{loadReceipt(unknown);});++tests;
        std::atomic_bool cancel{true};CaptureRequest request;request.live=false;request.n=1024;request.frames=1;request.replay={a};request.output=out/"cancelled";
        auto stopped=capture(request,resources,cancel);check(stopped.cancelled&&!stopped.hasData,"Cancelled capture returned data");++tests;
        request.frames=2;request.output=out/"bad_frames";rejects([&]{capture(request,resources,cancel);});++tests;
        cancel.store(false);request.frames=1;request.archive=false;request.output=out/"must_not_exist";
        auto volatileFrame=capture(request,resources,cancel);check(volatileFrame.hasData&&!fs::exists(request.output),"View-only capture wrote files");
        check(volatileFrame.measurement.average==measure({pa}).average,"View-only changed measurements");++tests;
        cancel.store(true);WaveRequest w;w.output=out/"cancelled_wave.bin";rejects([&]{generateWaveform(w,cancel);});check(!fs::exists(w.output),"Cancelled waveform was published");++tests;
        w.peak=40000;cancel.store(false);rejects([&]{generateWaveform(w,cancel);});++tests;
        std::cout<<"CORE_TESTS_PASS checks="<<tests<<" no hardware access\n";return 0;
    }catch(const std::exception &e){std::cerr<<"CORE_TESTS_FAIL: "<<e.what()<<'\n';return 2;}
}
