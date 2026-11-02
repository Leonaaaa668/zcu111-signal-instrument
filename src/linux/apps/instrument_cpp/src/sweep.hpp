#pragma once
#include "core.hpp"
namespace instrument {
struct SweepRequest {double start=1.3e9,stop=2.3e9,targetSeconds=0;unsigned n=8192,frames=1;bool archive=false;fs::path output;};
struct SweepResult {
 double start=0,stop=0,seconds=0,spacing=0,enbw=0,targetSeconds=0;
 unsigned fftN=0,averages=0;bool complete=false;
 std::vector<double> frequency,dbfs;
 std::vector<double> centers,segmentMs;
 std::vector<std::uint64_t> firstSequence,lastSequence;
 std::vector<unsigned> effectiveFrames,groups;
};
std::vector<std::uint64_t> planSweep(double start,double stop);
void appendSweepSlice(SweepResult &out,const Measurement &m,double low,double high,bool includeHigh);
bool sweepNeedsMore(unsigned groups,double elapsedSeconds,double targetSeconds,unsigned segments);
SweepResult sweepSpectra(const SweepRequest &request,const Resources &resources,std::atomic_bool &cancel,
 const std::function<void(unsigned,unsigned)> &progress={});
}
