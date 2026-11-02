#include "core.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
namespace {std::atomic_bool cancelled{false};void handler(int){cancelled.store(true);}}
int main(int argc,char **argv){try{
    if(argc<4||argc>5)throw std::runtime_error("Usage: zcu111-stream <resources> <N> <batches> [linear-averages]; no files saved");
    auto resources=instrument::loadResources(argv[1]);instrument::CaptureRequest request;
    request.n=unsigned(std::stoul(argv[2]));const auto limit=std::stoull(argv[3]);
    if(!limit)throw std::runtime_error("Batch count must be positive");
    if(argc==5)request.frames=unsigned(std::stoul(argv[4]));
    std::signal(SIGTERM,handler);std::signal(SIGINT,handler);
    const auto begin=std::chrono::steady_clock::now();std::uint64_t completed=0;double read=0,validate=0,measure=0;
    instrument::streamSpectra(request,resources,cancelled,[&](instrument::CaptureResult &&r,std::uint64_t count){
        completed=count;read+=r.readMs;validate+=r.validateMs;measure+=r.measureMs;
        if(count==1||count%10==0||count==limit){
            const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
            std::cout<<"{\"batches\":"<<count<<",\"seconds\":"<<seconds<<",\"spectra_per_s\":"<<count/seconds
            <<",\"read_ms\":"<<read/count<<",\"validate_ms\":"<<validate/count<<",\"measure_ms\":"<<measure/count
            <<",\"sequence\":"<<r.lastSequence<<",\"tone_hz\":"<<r.measurement.toneHz
            <<",\"archived\":false,\"gapless_iq\":false}"<<std::endl;
        }
        if(count>=limit)cancelled.store(true);
    });
    return completed==limit?0:3;
}catch(const std::exception&e){std::cerr<<"STREAM ERROR: "<<e.what()<<'\n';return 2;}}
