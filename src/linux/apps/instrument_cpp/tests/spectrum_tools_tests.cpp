#include "spectrum_tools.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace instrument;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Invalid spectrum-tool input accepted");}
int main(){try{
 auto off=planVideoAverages(3433.2,1,0);check(off.frames==1&&off.equivalentHz==3433.2,"Auto must preserve one-frame capture");
 auto manual=planVideoAverages(3433.2,1,1000);check(manual.frames==3&&manual.equivalentHz<=1000,"VBW frame planning wrong");
 check(planVideoAverages(3433.2,16,1000).frames==16,"VBW discarded explicit linear averages");
 check(planVideoAverages(27465.8,1,1000).frames==12,"Wide RBW not coupled to VBW work");
 reject([]{planVideoAverages(219726,1,1);});reject([]{planVideoAverages(3433,0,1000);});reject([]{planVideoAverages(3433,1,-1);});
 std::vector<double> f(101),db(101,-100);for(unsigned i=0;i<f.size();++i)f[i]=1.79e9+i*1000;
 db[10]=-20;db[30]=-30;db[50]=-40;db[80]=-50;
 MarkerBank markers;markers.setTrace(f,db,f.front(),f.back());check(markers.points[0].frequency==f[10],"Initial tracking marker not at peak");
 markers.position(0,f[30]+400);check(markers.points[0].frequency==f[30]&&!markers.points[0].tracking,"Manual marker did not snap or stop tracking");
 auto changed=db;changed[10]=-10;changed[30]=-31;markers.setTrace(f,changed,f.front(),f.back());check(markers.points[0].frequency==f[30]&&markers.points[0].level==-31,"Fixed marker jumped on refresh");
 markers.setView(f[20],f[90]);markers.peak(0);check(markers.points[0].frequency==f[30],"Peak Search escaped visible span");
 markers.setView(f.front(),f.back());check(markers.fourPeaks(3000)==4,"Four separated peaks not found");
 markers.reference=0;auto delta=markers.delta(1);check(delta.valid&&delta.frequency==20000&&delta.level==-21,"Signed delta Hz/dB incorrect");
 markers.enable(0,false);check(!markers.delta(1).valid,"Disabled reference produced a delta");
 markers.enable(0,true);markers.position(1,1.3e9);check(!markers.points[1].valid,"Out-of-range marker got a fabricated level");
 markers.setTrace(f,std::vector<double>(101,-100),f.front(),f.back());check(markers.fourPeaks(3000)==0,"Flat trace fabricated peaks");
 reject([&]{markers.position(4,1.79e9);});auto bad=f;bad[3]=bad[2];reject([&]{markers.setTrace(bad,db,f.front(),f.back());});
 // Explicit irregular RF grid as used at stitched segment seams.
 MarkerBank irregular;std::vector<double> rf={1300009765.625,1300028076.171875,1380000000,1380015000};
 irregular.setTrace(rf,{-60,-50,-40,-30},1.3e9,1.4e9);irregular.position(0,rf[1]);irregular.position(1,rf[3]);irregular.reference=0;
 check(irregular.delta(1).frequency==rf[3]-rf[1]&&irregular.delta(1).level==20,"Swept marker used a fabricated uniform RF grid");
 std::cout<<"SPECTRUM_TOOLS_PASS video-equivalence workload/range,4markers,visible peak,fixed-position refresh,signed delta,disabled/outside reference,stitched RF grid\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
