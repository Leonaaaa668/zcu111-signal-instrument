#include "core.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
namespace {std::atomic_bool cancelled{false};void stop(int){cancelled.store(true);}}
int main(int argc,char **argv){
    try{
        if(argc<3||argc>5)throw std::runtime_error("Usage: zcu111-wavegen <CW|PULSE|LFM|LFM_PULSE|BARKER13|PSK|QAM> <new-file.bin> [order] [offset-MHz]");
        instrument::WaveRequest request;request.type=argv[1];request.output=argv[2];
        if(request.type=="QAM"||request.type=="PSK")request.offset=0;
        if(request.type=="PSK")request.order=4;
        if(argc>=4)request.order=unsigned(std::stoul(argv[3]));
        if(argc>=5)request.offset=std::stod(argv[4])*1e6;
        std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
        instrument::generateWaveform(request,cancelled);
        std::cout<<"WAVEFORM_SAVED_NOT_PLAYING bytes=48000000 C++_ONLY\n";return 0;
    }catch(const std::exception &e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 2;}
}
