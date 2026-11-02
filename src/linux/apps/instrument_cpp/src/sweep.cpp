#include "sweep.hpp"
#include <cmath>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <iomanip>
namespace instrument {
std::vector<std::uint64_t> planSweep(double start,double stop){
 if(!std::isfinite(start)||!std::isfinite(stop)||start<1.3e9||stop>2.3e9||stop-start<1e6||stop-start>1e9)throw std::runtime_error("Swept interval must lie within1300..2300MHz, span1..1000MHz");
 std::vector<std::uint64_t> out;
 if(stop-start<=80e6){out.push_back(std::uint64_t(std::llround((start+stop)/2)));return out;}
 auto center=start+40e6;
 for(unsigned i=0;i<16;++i){out.push_back(std::uint64_t(std::llround(center)));if(center+50e6>=stop)return out;center=std::min(center+80e6,stop-40e6);}
 throw std::runtime_error("Invalid sweep plan");
}
void appendSweepSlice(SweepResult &out,const Measurement &m,double low,double high,bool includeHigh){
 if(!m.n||m.displayDbfs.size()!=m.n||m.fs!=150e6||!std::isfinite(low)||!std::isfinite(high)||high<=low||low<m.center-50e6-.5||high>m.center+50e6+.5)throw std::runtime_error("Sweep slice geometry mismatch");
 if(out.fftN&&((out.fftN!=m.n)||std::abs(out.spacing-m.spacing)>1e-6))throw std::runtime_error("Mixed FFT/averaging in sweep");
 out.fftN=m.n;if(!out.averages)out.averages=m.frames;out.spacing=m.spacing;out.enbw=m.enbwHz;
 // Keep the actual RF grid of each FFT, rather than interpolating logarithmic
 // power or treating sequential segments as one simultaneously sampled FFT.
 for(unsigned i=0;i<m.n;++i){const double f=m.center-m.fs/2+i*m.spacing;if(f<low||f>high||(!includeHigh&&f==high))continue;
  if(!std::isfinite(m.displayDbfs[i])||(!out.frequency.empty()&&f<=out.frequency.back()))throw std::runtime_error("Invalid/nonmonotonic stitched spectrum");
  out.frequency.push_back(f);out.dbfs.push_back(m.displayDbfs[i]);}
}
bool sweepNeedsMore(unsigned groups,double elapsed,double target,unsigned segments){
 if(!std::isfinite(elapsed)||elapsed<0||!std::isfinite(target)||target<0||target>60||!segments)throw std::runtime_error("Invalid sweep time");
 return groups==0||(target>0&&elapsed<target/segments);
}
SweepResult sweepSpectra(const SweepRequest &request,const Resources &resources,std::atomic_bool &cancel,const std::function<void(unsigned,unsigned)> &progress){
 const auto centers=planSweep(request.start,request.stop);
 if(!resources.windows.count(request.n)||request.frames<1||request.frames>256)throw std::runtime_error("Invalid sweep FFT/average");
 SweepResult result;result.start=request.start;result.stop=request.stop;result.targetSeconds=request.targetSeconds;result.averages=request.frames;
 sweepNeedsMore(0,0,request.targetSeconds,unsigned(centers.size()));
 if(cancel.load())return result;
 if(request.archive){const auto raw=centers.size()*request.frames*(256ULL+8ULL*request.n);
  // Include per-segment spectrum CSVs, composite CSV and receipt/JSON overhead,
  // not just binary packet storage (especially important at Average1).
  const auto needed=raw+request.n*(220ULL*centers.size()+400ULL);
  if(fs::space(request.output.parent_path()).available<needed+needed/10+32ULL*1024*1024)throw std::runtime_error("Insufficient space for archived sweep segments");}
 if(request.archive&&!fs::create_directory(request.output))throw std::runtime_error("Sweep output already exists or parent absent");
 const auto started=std::chrono::steady_clock::now();
 for(unsigned i=0;i<centers.size()&&!cancel.load();++i){
  CaptureRequest captureRequest;captureRequest.centerHz=centers[i];captureRequest.n=request.n;captureRequest.frames=request.frames;captureRequest.archive=request.archive;
  const auto begin=std::chrono::steady_clock::now();
  Measurement accumulated;unsigned groups=0;std::uint64_t firstSequence=0,lastSequence=0;
  fs::path segment;
  if(request.archive){segment=request.output/("segment_"+std::to_string(i));if(!fs::create_directory(segment))throw std::runtime_error("Segment output already exists");}
  // Each segment gets the requested dwell budget. Keep collecting independently
  // validated batches; do not sleep to manufacture a reported sweep duration.
  while(!cancel.load()&&sweepNeedsMore(groups,std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count(),request.targetSeconds,unsigned(centers.size()))){
   if(request.archive){
    const auto needed=request.frames*(256ULL+8ULL*request.n)+request.n*220ULL;
    if(fs::space(segment).available<needed+needed/10+32ULL*1024*1024)throw std::runtime_error("Insufficient storage during manual sweep dwell");
    captureRequest.output=segment/("group_"+std::to_string(groups));
   }
   const auto captured=capture(captureRequest,resources,cancel);
   if(cancel.load()||captured.cancelled||!captured.hasData)return result;
   if(captured.measurement.center!=double(centers[i]))throw std::runtime_error("Sweep received a wrong-center segment");
   if(groups&&captured.firstSequence<=lastSequence)throw std::runtime_error("Stale frame sequence in sweep dwell");
   if(!groups)firstSequence=captured.firstSequence;
   lastSequence=captured.lastSequence;mergeMeasurements(accumulated,captured.measurement);++groups;
  }
  if(cancel.load()||!groups)return result;
  const double low=i?(double(centers[i-1])+centers[i])/2:request.start;
  const double high=i+1<centers.size()?(double(centers[i])+centers[i+1])/2:request.stop;
  appendSweepSlice(result,accumulated,low,high,i+1==centers.size());
  result.centers.push_back(double(centers[i]));result.firstSequence.push_back(firstSequence);result.lastSequence.push_back(lastSequence);
  result.effectiveFrames.push_back(accumulated.frames);result.groups.push_back(groups);
  result.segmentMs.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
  if(progress)progress(i+1,unsigned(centers.size()));
 }
 if(cancel.load()||result.centers.size()!=centers.size())return result;
 result.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();result.complete=true;
 if(request.archive){
  std::ostringstream csv;csv<<std::setprecision(17)<<"rf_hz,coherent_dbfs_per_bin\n";for(unsigned i=0;i<result.frequency.size();++i)csv<<result.frequency[i]<<','<<result.dbfs[i]<<'\n';
  auto text=csv.str();writeFile(request.output/"sweep.csv",Bytes(text.begin(),text.end()));
  result.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
  std::ostringstream meta;meta<<std::setprecision(17)<<"{\"kind\":\"SEQUENTIAL_NCO_SWEPT_SPECTRUM\",\"complete\":true,\"simultaneous\":false,\"calibrated_dbm\":false,\"start_hz\":"<<result.start<<",\"stop_hz\":"<<result.stop<<",\"seconds\":"<<result.seconds<<",\"target_seconds\":"<<result.targetSeconds<<",\"time_mode\":\""<<(result.targetSeconds>0?"MANUAL_MINIMUM_DWELL":"AUTO")<<"\",\"fft_n\":"<<result.fftN<<",\"averages\":"<<result.averages<<",\"segments\":[";
  for(unsigned i=0;i<centers.size();++i){if(i)meta<<',';meta<<"{\"center_hz\":"<<centers[i]<<",\"first_sequence\":"<<result.firstSequence[i]<<",\"last_sequence\":"<<result.lastSequence[i]<<",\"elapsed_ms\":"<<result.segmentMs[i]<<",\"effective_frames\":"<<result.effectiveFrames[i]<<",\"groups\":"<<result.groups[i]<<'}';}meta<<"]}\n";
  text=meta.str();writeFile(request.output/"sweep.json",Bytes(text.begin(),text.end()));
 }
 return result;
}
}
