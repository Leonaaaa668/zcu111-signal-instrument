#include "core.hpp"
#include "tuning.hpp"
#include "sweep.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
using namespace instrument;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
template<class F>void reject(F test){bool failed=false;try{test();}catch(const std::exception&){failed=true;}check(failed,"invalid tuning/sweep accepted");}
int main(int argc,char **argv){try{
 check(argc==2,"resources required");
 check(rxExpectedWord(1800000000)==0x400000000000ULL,"canonical NCO changed");
 for(auto center:{1300000000ULL,1310000000ULL,1800000000ULL,2300000000ULL}){auto word=rxExpectedWord(center);check(rxWordMatches(center,word)&&rxWordMatches(center,word+1)&&!rxWordMatches(center,word+3),"NCO mapping/tolerance");}
 reject([]{rxExpectedWord(1299999999);});reject([]{planSweep(1.2e9,2.3e9);});reject([]{planSweep(2.3e9,1.3e9);});
 auto plan=planSweep(1.3e9,2.3e9);check(plan.size()==13,"unexpected full sweep coverage");
 SweepResult result;result.start=1.3e9;result.stop=2.3e9;
 for(unsigned i=0;i<plan.size();++i){Measurement m;m.n=8192;m.frames=1;m.center=double(plan[i]);m.spacing=m.fs/m.n;m.enbwHz=1.5*m.spacing;m.displayDbfs.assign(m.n,-70);
  double low=i?(double(plan[i-1])+plan[i])/2:result.start,high=i+1<plan.size()?(double(plan[i])+plan[i+1])/2:result.stop;
  appendSweepSlice(result,m,low,high,i+1==plan.size());}
 check(result.frequency.front()-result.start<result.spacing&&result.stop-result.frequency.back()<result.spacing,"scan endpoints missing");
 for(unsigned i=1;i<result.frequency.size();++i)check(result.frequency[i]>result.frequency[i-1]&&result.frequency[i]-result.frequency[i-1]<2*result.spacing+.001&&result.dbfs[i]==-70,"stitch gap/duplicate/level change");
 auto resources=loadResources(argv[1]);auto file=fs::path(argv[1])/"fixtures/lower/spectrum.bin";auto bytes=readFile(file);auto receipt=loadReceipt(file.string()+".receipt.json");
 auto put=[&](unsigned index,std::uint32_t value){for(unsigned b=0;b<4;++b)bytes[4*index+b]=std::uint8_t(value>>(8*b));};
 auto word=rxExpectedWord(1310000000);put(20,1310000000);put(21,0);put(22,std::uint32_t(word));put(23,std::uint32_t(word>>32));put(9,2);receipt.ncoSeq=2;
 auto tuned=decode(bytes,receipt,resources);check(measure({tuned}).center==1310000000,"measurement ignored packet center");
 put(9,3);receipt.ncoSeq=3;auto newer=decode(bytes,receipt,resources);reject([&]{measure({tuned,newer});});
 put(20,1800000000);reject([&]{decode(bytes,receipt,resources);});
 std::cout<<"TUNING_SWEEP_TEST_PASS canonical/zone2 NCO, endpoint coverage, no seam duplicates, real RF grid, wrong-center and mixed-generation rejection\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
