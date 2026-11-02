#include "core.hpp"
#include <iostream>
#include <stdexcept>
#include <csignal>

namespace {std::atomic_bool cancelled{false};void signalHandler(int){cancelled.store(true);}}
int main(int argc,char **argv) {
    try {
        if(argc<4)throw std::runtime_error("Usage: zcu111-measure <resource-directory> <new-output-directory> <packet.bin>... | --live <N> <frames>");
        std::signal(SIGINT,signalHandler);std::signal(SIGTERM,signalHandler);
        auto resources=instrument::loadResources(argv[1]);instrument::CaptureRequest r;r.output=argv[2];
        if(std::string(argv[3])=="--live") {
            if(argc!=6&&argc!=7)throw std::runtime_error("Live needs N, frames and optional center Hz");
            r.n=unsigned(std::stoul(argv[4]));r.frames=unsigned(std::stoul(argv[5]));
            if(argc==7)r.centerHz=std::stoull(argv[6]);
        } else {
            r.live=false;for(int k=3;k<argc;++k)r.replay.emplace_back(argv[k]);
            const auto receipt=instrument::loadReceipt(r.replay.front().string()+".receipt.json");
            if(receipt.logn>16)throw std::runtime_error("Invalid log N");
            r.n=1u<<receipt.logn;r.frames=unsigned(r.replay.size());
        }
        auto result=instrument::capture(r,resources,cancelled);
        if(result.hasData)std::cout<<instrument::measurementJson(result.measurement);
        else std::cout<<"{\"cancelled\":true,\"frames\":0}\n";
        return result.cancelled?3:0;
    } catch(const std::exception &e) {std::cerr<<"ERROR: "<<e.what()<<'\n';return 2;}
}
