#include "core.hpp"
#include "tx_power_qt.hpp"
#include <chrono>
#include <iostream>
using namespace instrument;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
template<class F>void reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad,"Invalid context accepted");}
int main(int argc,char **argv){try{
 require(argc==3,"Need resources/output");const QString root=QString::fromLocal8Bit(argv[1]);
 const auto base=loadTxCwCalibration(root+"/tx_cw_calibration.json",root+"/tx_cw_verification.json");
 const auto context=loadTxCw4025gContext(base,root+"/tx_cw_4025g_context.json",root+"/tx_cw_4025g_verification.json");
 require(context.commandOffsetDb==.06&&context.minimumDbm==-40&&context.maximumDbm==-20,"Context scope lost");
 require(context.profileSha==base.profileSha&&context.setupId!=base.setupId&&context.referencePlane!=base.referencePlane,"Original/corrected reference confused");
 require(txCwRms(context,1.79e9,-25)==txCwRms(base,1.79e9,-24.94),"Correction does not reproduce tested original command");
 require(std::abs(txCwPowerDbm(context,txCwRms(context,1.79e9,-25))+25)<1e-10,"Context prediction inverse");
 require(dbwFromDbm(context.minimumDbm)==-70&&dbwFromDbm(context.maximumDbm)==-50,"Context dBW bounds");
 reject([&]{txCwRms(context,1.79e9,-40.001);});reject([&]{txCwRms(context,1.79e9,-19.999);});reject([&]{txCwRms(context,1.8e9,-25);});
 auto missingProof=base;missingProof.commandOffsetDb=.06;reject([&]{txCwRms(missingProof,1.79e9,-25);});
 const auto out=fs::path(argv[2])/("run_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(out);
 QFile profileFile(root+"/tx_cw_4025g_context.json"),proofFile(root+"/tx_cw_4025g_verification.json");require(profileFile.open(QIODevice::ReadOnly)&&proofFile.open(QIODevice::ReadOnly),"No context resources");
 auto profile=QJsonDocument::fromJson(profileFile.readAll()).object(),proof=QJsonDocument::fromJson(proofFile.readAll()).object();
 auto put=[&](const fs::path &p,const QJsonObject &o){const auto b=QJsonDocument(o).toJson();writeFile(p,Bytes(b.begin(),b.end()));return QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex();};
 const auto badProfile=out/"bad.json",badProof=out/"bad-proof.json";
 auto wrongOffset=profile;wrongOffset["command_offset_db"]=.5;put(badProfile,wrongOffset);reject([&]{loadTxCw4025gContext(base,QString::fromStdString(badProfile.string()),root+"/tx_cw_4025g_verification.json");});
 auto failedPoint=proof["independent_corrected_check"].toObject();failedPoint["readings_dbm"]=QJsonArray{-24.5,-24.5,-24.5};proof["independent_corrected_check"]=failedPoint;
 const auto badProfile2=out/"bad-point-profile.json";profile["verification_sha256"]=QString::fromLatin1(put(badProof,proof));put(badProfile2,profile);
 reject([&]{loadTxCw4025gContext(base,QString::fromStdString(badProfile2.string()),QString::fromStdString(badProof.string()));});
 std::atomic_bool cancel{false};WaveRequest request;request.targetDbm=-25;request.powerModel=context;request.output=out/"desired_minus25.bin";generateWaveform(request,cancel);
 const auto generated=readFile(request.output,64*1024*1024);const auto generatedSha=sha256(generated);
 // Fingerprint of the exact waveform played for the user-reported4025G check.
 require(generatedSha=="e3f799c23b31b7fb9baa7f5ce7510acda653ea6240372a2aa3aa7a352ec88067","Corrected waveform differs from physically tested original command");
 const auto bytes=readFile(request.output.string()+".json");const auto meta=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(bytes.data()),int(bytes.size()))).object();
 require(meta["requested_dbm"].toDouble()==-25&&meta["base_model_command_dbm"].toDouble()==-24.94,"Metadata target/command confused");
 require(meta["tx_context_profile_sha256"].toString().toStdString()==context.contextSha&&meta["power_reference_plane"].toString().toStdString()==context.referencePlane,"Correction provenance missing");
 std::cout<<"CW_CONTEXT_PASS range/dBW, original reference preserved, failed evidence rejected, 10ms48MB SHA identical to physical4025G-tested waveform; NO RF ACTION\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 2;}}
