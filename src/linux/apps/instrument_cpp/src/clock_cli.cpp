#include "clock.hpp"
#include <iostream>
int main(int argc,char **argv){
    try{
        if(argc==2&&std::string(argv[1])=="--list-adapters"){std::cout<<instrument::listClockAdapters();return 0;}
        if(argc==4&&std::string(argv[1])=="--plan"){
            auto steps=instrument::clockPlan(argv[2]);auto json=instrument::clockPlanJson(steps);instrument::writeFile(argv[3],instrument::Bytes(json.begin(),json.end()));std::cout<<"CLOCK_PLAN_READY steps="<<steps.size()<<" NO_HARDWARE_WRITES\n";return 0;
        }
        if(argc==5&&std::string(argv[1])=="--apply"&&std::string(argv[4])=="--outputs-stopped"){
            instrument::applyClockPlan(argv[3],instrument::clockPlan(argv[2]));std::cout<<"CLOCK_WRITES_COMPLETED; PLL_LOCK_UNVERIFIED; RFDC_STARTUP_STILL_REQUIRED\n";return 0;
        }
        throw std::runtime_error("Usage: zcu111-clock --list-adapters | --plan <clock-resources> <new-json> | --apply <clock-resources> /dev/i2c-N --outputs-stopped");
    }catch(const std::exception &error){std::cerr<<"ERROR: "<<error.what()<<'\n';return 2;}
}
