#ifdef INSTRUMENT_POLICY_ONLY
#include "tx_power.hpp"
#else
#include "core.hpp"
#endif
#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace instrument;
namespace {
void check(bool yes,const char *message){if(!yes)throw std::runtime_error(message);}
template<class F>void reject(F fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}check(rejected,"Rejected input was accepted");}
}
int main(int argc,char **argv){try{
#ifdef INSTRUMENT_POLICY_ONLY
 (void)argc;(void)argv;
#else
 check(argc==2,"One output directory is required");const fs::path base=argv[1];fs::create_directories(base);const fs::path out=base/("run_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(!fs::exists(out),"Preserve earlier evidence");fs::create_directory(out);
#endif
 // Numbers copied from the frozen1790MHz reference; no synthesized RF observations.
 TxCwCalibration m;m.profileSha="5dea09fa5050d3bda273107ba2daa3f42d2c91429a1fa9faaca1f1a6290c0f56";m.verificationSha=std::string(64,'a');m.setupId="digital_contract_fixture";m.referencePlane="test_only_not_RF_measurement";m.slope=1.000293139719282;m.dbmAt2048=-33.43762976117503;m.minRms=255.94061811287398;m.maxRms=32700.064489233046;
 const double original=txCwRms(m,1.79e9,-40);
 for(double f:{1.3e9,1.5e9,1.79e9,1.8e9,2.e9,2.3e9})check(txCwRms(m,f,-40,true)==original,"Estimate changed reference-amplitude calculation");
 reject([&]{txCwRms(m,1.3e9,-40);});reject([&]{txCwRms(m,2.3e9,-40);});
 for(double f:{1.3e9-1,2.3e9+1,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})reject([&]{txCwRms(m,f,-40,true);});
 for(double dbm:{-50.001,-9.999,std::numeric_limits<double>::quiet_NaN()})reject([&]{txCwRms(m,1.3e9,dbm,true);});
 auto context=m;context.contextSha=std::string(64,'b');context.contextVerificationSha=std::string(64,'c');context.commandOffsetDb=.06;context.minimumDbm=-40;context.maximumDbm=-20;
 check(txCwRms(context,1.3e9,-25,true)==txCwRms(context,1.79e9,-25),"Context compensation changed with estimate policy");reject([&]{txCwRms(context,1.3e9,-40.01,true);});reject([&]{txCwRms(context,2.3e9,-19.99,true);});
#ifdef INSTRUMENT_POLICY_ONLY
 std::cout<<"CW_FREQUENCY_MODEL_POLICY_PASS band_edges,strict_default,invalid_frequency_and_power,context_offset_and_ranges; WAVEFORM_AND_QT_NOT_TESTED\n";return 0;
#else
 std::atomic_bool cancelled{false};WaveRequest wave;wave.powerModel=context;wave.targetDbm=-25;wave.output=out/"forbidden_without_policy.bin";wave.offset=500e6;
 reject([&]{generateWaveform(wave,cancelled);});check(!fs::exists(wave.output.string()+".part"),"Strict rejection published a partial record");
 wave.allowPowerFrequencyEstimate=true;
 for(double f:{1.3e9,1.8e9,2.3e9}){
  wave.offset=1.8e9-f;wave.output=out/("cw_"+std::to_string(int(f/1e6))+"MHz.bin");generateWaveform(wave,cancelled);
  check(fs::file_size(wave.output)==48000000,"Incomplete10ms waveform");
  const auto bytes=readFile(wave.output.string()+".json");const std::string meta(bytes.begin(),bytes.end());
  check(meta.find("\"power_control\":\"CW_MODEL_ESTIMATED_DBM\"")!=std::string::npos&&meta.find("\"power_frequency_estimated\":true")!=std::string::npos,"Off-reference file misrepresented as measured reference");
 }
 wave.type="QAM";wave.output=out/"qam_rejected.bin";reject([&]{generateWaveform(wave,cancelled);});
 std::cout<<"CW_FREQUENCY_ESTIMATE_PASS strict_default,band_edges,invalid_inputs,context_range,three10ms_files,estimated_metadata; NO RF ACTION\n";return 0;
#endif
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 2;}}
