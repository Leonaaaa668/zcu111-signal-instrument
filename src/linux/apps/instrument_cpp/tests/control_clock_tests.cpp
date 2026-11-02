#include "clock.hpp"
#include "control.hpp"
#include "storage.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace instrument;
namespace {
void require(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
void rejects(const std::function<void()> &action){try{action();}catch(const std::exception&){return;}throw std::runtime_error("Unsafe operation was accepted");}
}
int main(int argc,char **argv){try{
    require(argc==3,"Need resources and test-output root");const fs::path resources=argv[1],root=argv[2];fs::create_directories(root);
    const auto out=root/("run_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));require(fs::create_directory(out),"Cannot create unique test evidence");unsigned checks=0;
    RepeatGate gate;const auto first=gate.begin(true);require(gate.permits(first),"Repeat token rejected");gate.stop();require(!gate.permits(first)&&!gate.enabled(),"Stop did not cancel queued repeat");++checks;
    const auto second=gate.begin(true);require(!gate.permits(first)&&gate.permits(second),"Old timer can start a new session");++checks;
    const auto single=gate.begin(false);require(!gate.permits(second)&&!gate.permits(single),"Single operation left repetition active");++checks;
    const auto destination=out/"kept.bin";writeFile(destination,Bytes{'k','e','e','p'});rejects([&]{writeFile(destination,Bytes{'X'});});require(readFile(destination)==Bytes({'k','e','e','p'}),"Existing file overwritten");++checks;
    const auto partial=out/"other.part";writeFile(partial,Bytes{'n','e','w'});rejects([&]{publishNew(partial,destination);});require(fs::exists(partial)&&readFile(destination)==Bytes({'k','e','e','p'}),"No-replace publication clobbered a file");++checks;
    const auto published=out/"published.bin";publishNew(partial,published);require(!fs::exists(partial)&&readFile(published)==Bytes({'n','e','w'}),"Publication failed");++checks;
    {ExclusiveOutput firstWriter(out/"owned.part");rejects([&]{ExclusiveOutput secondWriter(out/"owned.part");});firstWriter.write("OK",2);firstWriter.finish();}require(readFile(out/"owned.part")==Bytes({'O','K'}),"Competing writer damaged output");++checks;
    const auto steps=clockPlan(resources/"clock");require(steps.size()==264,"Clock plan length differs");unsigned writes=0,elapsed=0;std::ostringstream canonical;
    for(const auto &s:steps){canonical<<s.kind<<'\t';for(auto byte:s.data)canonical<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);canonical<<'\t'<<std::dec<<s.delayMs<<'\n';}
    const auto expected=readFile(resources/"clock/plan_reference.tsv");require(canonical.str()==std::string(expected.begin(),expected.end()),"Full clock message stream differs from frozen reference");++checks;
    ClockTransport transport{[&](const Bytes &data){++writes;return data.size();},[&](unsigned ms){elapsed+=ms;}};replayClockPlan(steps,transport);require(writes==259&&elapsed==12495,"Clock reset/delay sequence differs");++checks;
    unsigned issued=0,slept=0;ClockTransport shortWrite{[&](const Bytes &data){return ++issued==3?data.size()-1:data.size();},[&](unsigned){++slept;}};
    rejects([&]{replayClockPlan(steps,shortWrite);});require(issued==3&&slept==2,"Clock programming continued after short write");++checks;
    issued=0;ClockTransport busy{[&](const Bytes&)->std::size_t{++issued;throw std::runtime_error("EBUSY");},[](unsigned){}};rejects([&]{replayClockPlan(steps,busy);});require(issued==1,"Busy clock transport retried or forced");++checks;
    fs::create_directory(out/"bad_clock");auto bad=readFile(resources/"clock/clock_tables.h");bad.back()^=1;writeFile(out/"bad_clock/clock_tables.h",bad);rejects([&]{clockPlan(out/"bad_clock");});++checks;
    std::cout<<"CONTROL_CLOCK_STORAGE_PASS checks="<<checks<<" NO_HARDWARE_WRITES evidence="<<out.string()<<'\n';return 0;
}catch(const std::exception &error){std::cerr<<"CONTROL_CLOCK_STORAGE_FAIL: "<<error.what()<<'\n';return 2;}}
