#include "spectrum_tools.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace instrument {
namespace {void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}void idOk(unsigned id){require(id<4,"Marker index outside M1..M4");}}
double equivalentVideoBandwidth(double rbw,unsigned frames){require(std::isfinite(rbw)&&rbw>0&&frames>0,"Invalid video-average geometry");return frames==1?rbw:std::min(rbw,.38*rbw/(frames-.8));}
VideoPlan planVideoAverages(double rbw,unsigned minimumFrames,double requested){
 require(std::isfinite(rbw)&&rbw>0&&minimumFrames>=1&&minimumFrames<=256&&std::isfinite(requested)&&requested>=0,"Invalid VBW/average setting");
 unsigned frames=minimumFrames;
 if(requested>0&&requested<rbw){const double required=std::ceil(.8+.38*rbw/requested);require(required<=256,"Equivalent VBW requires >256 frames; increase VBW or choose narrower RBW");frames=std::max(frames,unsigned(std::max(1.,required)));}
 return {frames,requested,equivalentVideoBandwidth(rbw,frames),rbw};
}
unsigned MarkerBank::nearest(double frequency)const{
 require(!frequencies.empty(),"No trace for Marker");const auto it=std::lower_bound(frequencies.begin(),frequencies.end(),frequency);
 if(it==frequencies.begin()){return 0;}
 if(it==frequencies.end()){return unsigned(frequencies.size()-1);}
 const auto index=unsigned(it-frequencies.begin());return frequency-frequencies[index-1]<=frequencies[index]-frequency?index-1:index;
}
void MarkerBank::setTrace(const std::vector<double> &rf,const std::vector<double> &db,double viewLow,double viewHigh){
 require(rf.size()==db.size()&&rf.size()>=2,"Invalid marker trace size");
 for(unsigned i=0;i<rf.size();++i)require(std::isfinite(rf[i])&&std::isfinite(db[i])&&(!i||rf[i]>rf[i-1]),"Invalid/nonmonotonic marker trace");
 frequencies=rf;levels=db;setView(viewLow,viewHigh);
}
void MarkerBank::setView(double viewLow,double viewHigh){require(std::isfinite(viewLow)&&std::isfinite(viewHigh)&&viewLow<viewHigh,"Invalid marker view");low=viewLow;high=viewHigh;for(unsigned i=0;i<4;++i)refresh(i);}
void MarkerBank::refresh(unsigned id){auto &m=points[id];m.valid=false;if(!m.enabled||frequencies.empty())return;
 if(m.tracking||!m.positioned){const auto a=std::lower_bound(frequencies.begin(),frequencies.end(),low),b=std::upper_bound(frequencies.begin(),frequencies.end(),high);if(a==b)return;
  const auto first=unsigned(a-frequencies.begin()),last=unsigned(b-frequencies.begin());const auto k=unsigned(std::max_element(levels.begin()+first,levels.begin()+last)-levels.begin());m.frequency=frequencies[k];m.positioned=true;
 }
 const auto step=std::max(frequencies[1]-frequencies[0],frequencies.back()-frequencies[frequencies.size()-2]);
 if(m.frequency<frequencies.front()-step||m.frequency>frequencies.back()+step)return;
 const auto k=nearest(m.frequency);m.frequency=frequencies[k];m.level=levels[k];m.valid=true;
}
void MarkerBank::enable(unsigned id,bool on){idOk(id);points[id].enabled=on;refresh(id);}
void MarkerBank::position(unsigned id,double frequency){idOk(id);require(std::isfinite(frequency),"Invalid marker frequency");auto &m=points[id];m.enabled=true;m.tracking=false;m.positioned=true;m.frequency=frequency;refresh(id);}
void MarkerBank::track(unsigned id,bool on){idOk(id);points[id].tracking=on;refresh(id);}
void MarkerBank::peak(unsigned id){idOk(id);auto &m=points[id];m.enabled=true;const auto follow=m.tracking;m.tracking=true;refresh(id);m.tracking=follow;}
unsigned MarkerBank::fourPeaks(double separation){require(std::isfinite(separation)&&separation>0,"Invalid peak separation");std::vector<unsigned> candidates;
 for(unsigned i=0;i<frequencies.size();++i){if(frequencies[i]<low||frequencies[i]>high)continue;if(i&&levels[i]<levels[i-1])continue;if(i+1<levels.size()&&levels[i]<=levels[i+1])continue;if(!((i&&levels[i]>levels[i-1])||(i+1<levels.size()&&levels[i]>levels[i+1])))continue;candidates.push_back(i);}
 std::sort(candidates.begin(),candidates.end(),[&](unsigned a,unsigned b){return levels[a]!=levels[b]?levels[a]>levels[b]:frequencies[a]<frequencies[b];});
 std::vector<unsigned> found;for(auto k:candidates){if(std::any_of(found.begin(),found.end(),[&](unsigned p){return std::abs(frequencies[k]-frequencies[p])<separation;}))continue;found.push_back(k);if(found.size()==4)break;}
 for(unsigned i=0;i<4;++i){points[i].enabled=i<found.size();if(points[i].enabled)position(i,frequencies[found[i]]);else points[i].valid=false;}return unsigned(found.size());
}
MarkerDelta MarkerBank::delta(unsigned id)const{idOk(id);idOk(reference);const auto &m=points[id],&r=points[reference];if(!m.enabled||!r.enabled||!m.valid||!r.valid)return {};return {true,m.frequency-r.frequency,m.level-r.level};}
}
