#include "core.hpp"
#include "tx_power_qt.hpp"
#include "sweep.hpp"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <chrono>
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace instrument;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
template<class F>void reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"invalid power/time input accepted");}
int main(int argc,char **argv){try{
 check(argc==3,"resources and output required");
 const auto resources=fs::path(argv[1]);
 auto model=loadTxCwCalibration(QString::fromLocal8Bit(argv[1])+"/tx_cw_calibration.json",QString::fromLocal8Bit(argv[1])+"/tx_cw_verification.json");
 check(model.profileSha=="5dea09fa5050d3bda273107ba2daa3f42d2c91429a1fa9faaca1f1a6290c0f56","frozen model changed");
 const double counts[]={304,541,962,1711,3042,5409,9616,17098,30400};
 for(unsigned i=0;i<9;++i){const double dbm=-50.+5*i;check(std::abs(txCwRms(model,1.79e9,dbm)-counts[i])<.51,"inverse disagrees with independently measured amplitude table");check(std::abs(txCwPowerDbm(model,counts[i])-dbm)<.025,"power round trip");}
 check(dbmFromDbw(-70)==-40&&dbwFromDbm(-40)==-70,"dBW conversion wrong");
 reject([&]{txCwRms(model,1.8e9,-40);});reject([&]{txCwRms(model,1.79e9,-50.001);});reject([&]{txCwRms(model,1.79e9,-9.999);});
 auto bad=model;bad.referencePlane="\"bad";reject([&]{txCwRms(bad,1.79e9,-40);});
 check(sweepNeedsMore(0,0,0,13)&&!sweepNeedsMore(1,0,0,13),"Auto must take exactly one group");
 check(sweepNeedsMore(1,.099,1.3,13)&&!sweepNeedsMore(1,.1,1.3,13),"manual segment dwell deadline");
 reject([]{sweepNeedsMore(1,0,-1,13);});reject([]{sweepNeedsMore(1,0,61,13);});
 // Compare a merged 1+2 acquisition with one true three-frame mean.
 const auto r=loadResources(resources);const auto file=resources/"fixtures/lower/spectrum.bin";
 auto packet=decode(readFile(file),loadReceipt(file.string()+".receipt.json"),r);
 auto second=packet,third=packet;second.hash=std::string(64,'a');third.hash=std::string(64,'b');
 for(auto &v:second.power){v/=2;}
 for(auto &v:third.power){v/=4;}
 auto merged=measure({packet});const auto group=measure({second,third});mergeMeasurements(merged,group);const auto expected=measure({packet,second,third});
 check(merged.frames==3&&merged.hashes.size()==3,"dwell effective frame count lost");
 for(unsigned k=0;k<merged.n;++k)check(std::abs(merged.displayDbfs[k]-expected.displayDbfs[k])<1e-10,"dwell averaged logarithms or group counts incorrectly");
 check(std::abs(merged.totalDbfs-expected.totalDbfs)<1e-10,"merged derived power stale");
 reject([&]{mergeMeasurements(merged,group);});auto changed=group;++changed.ncoSequence;reject([&]{mergeMeasurements(merged,changed);});
 fs::create_directories(argv[2]);const auto out=fs::path(argv[2])/("run_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(out);
 WaveRequest wave;wave.output=out/"cw_minus40.bin";wave.powerModel=model;wave.targetDbm=-40;std::atomic_bool cancel{false};generateWaveform(wave,cancel);
 const auto data=readFile(wave.output,64*1024*1024);long double energy=0;unsigned peak=0;
 for(std::size_t k=0;k<data.size();k+=4){auto part=[&](unsigned j){return int(std::int16_t(unsigned(data[k+j])|(unsigned(data[k+j+1])<<8)));};const int i=part(0),q=part(2);energy+=double(i)*i+double(q)*q;peak=std::max({peak,unsigned(std::abs(i)),unsigned(std::abs(q))});}
 const auto rms=std::sqrt(double(energy/(data.size()/4)));check(peak<=32700&&std::abs(txCwPowerDbm(model,rms)+40)<.01,"generated IQ misses requested model power");
 const auto meta=readFile(wave.output.string()+".json");const auto json=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(meta.data()),int(meta.size()))).object();
 check(json["power_control"].toString()=="CW_MODEL_DBM"&&json["requested_dbm"].toDouble()==-40&&json["sha256"].toString().toStdString()==sha256(data),"metadata does not bind power request to IQ file");
 wave.type="QAM";wave.output=out/"rejected.bin";reject([&]{generateWaveform(wave,cancel);});check(!fs::exists(wave.output.string()+".part"),"uncalibrated modulation published as dBm");
 std::cout<<"PHYSICAL_CONTROLS_PASS frozen independent TX table, dBm/dBW, quantized 10ms IQ energy/hash, dwell timing and weighted linear power, stale/mixed NCO rejection; NO RF ACTION\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 2;}}
