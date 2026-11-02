#include "sweep.hpp"
#include <iostream>
#include <csignal>
#include <algorithm>
namespace{std::atomic_bool stop{false};void interrupted(int){stop.store(true);}}
int main(int argc,char **argv){try{
 if(argc!=8&&argc!=9)throw std::runtime_error("usage: zcu111-sweep resources new-output start-Hz stop-Hz N averages archive(0/1) [target-seconds,0=Auto]");
 instrument::SweepRequest request;request.output=argv[2];request.start=std::stod(argv[3]);request.stop=std::stod(argv[4]);request.n=unsigned(std::stoul(argv[5]));request.frames=unsigned(std::stoul(argv[6]));request.archive=std::string(argv[7])=="1";
 if(argc==9)request.targetSeconds=std::stod(argv[8]);
 std::signal(SIGINT,interrupted);std::signal(SIGTERM,interrupted);
 auto result=instrument::sweepSpectra(request,instrument::loadResources(argv[1]),stop,[](unsigned i,unsigned n){std::cerr<<"SWEEP_SEGMENT "<<i<<'/'<<n<<'\n';});
 if(!result.complete){std::cerr<<"SWEEP_CANCELLED_NO_COMPLETE_TRACE\n";return 3;}
 std::cout<<"SWEEP_COMPLETE segments="<<result.centers.size()<<" seconds="<<result.seconds<<" target_seconds="<<result.targetSeconds<<" min_frames="<<*std::min_element(result.effectiveFrames.begin(),result.effectiveFrames.end())<<" max_frames="<<*std::max_element(result.effectiveFrames.begin(),result.effectiveFrames.end())<<" points="<<result.frequency.size()<<" simultaneous=false\n";return 0;
}catch(const std::exception&e){std::cerr<<"SWEEP_ERROR: "<<e.what()<<'\n';return 2;}}
