#pragma once
#include "tx_power.hpp"
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QCryptographicHash>
#include <cmath>
#include <stdexcept>
inline instrument::TxCwCalibration loadTxCwCalibration(const QString &path,const QString &verificationPath){
 auto read=[](const QString &p){QFile f(p);if(!f.open(QIODevice::ReadOnly)||f.size()<=0||f.size()>256*1024)throw std::runtime_error("TX power profile/verification unavailable");return f.readAll();};
 const auto bytes=read(path),proof=read(verificationPath);const auto d=QJsonDocument::fromJson(bytes),v=QJsonDocument::fromJson(proof);
 auto check=[](bool ok){if(!ok)throw std::runtime_error("TX power profile/independent verification mismatch");};check(d.isObject()&&v.isObject());const auto o=d.object(),e=v.object();
 auto hash=[](const QByteArray &x){return QCryptographicHash::hash(x,QCryptographicHash::Sha256).toHex();};const auto sha=hash(bytes);
 check(o["kind"].toString()=="CW_ONLY"&&o["status"].toString()=="MEASURED_PROJECT_SCREENING_PASS"&&o["extrapolation"].toString()=="FORBIDDEN");
 check(e["status"].toString()=="INDEPENDENT_RELATIVE_POWER_CHECK_PASS"&&e["frozen_profile_sha256"].toString()==QString::fromLatin1(sha)&&e["absolute_accuracy_below_0p1_db"].toString()=="NOT_CLAIMED");
 auto number=[&](const QJsonObject &x,const char *key){check(x[key].isDouble()&&std::isfinite(x[key].toDouble()));return x[key].toDouble();};
 check(number(e,"worst_single_reading_error_db")>=0&&number(e,"worst_single_reading_error_db")<=.1);
 check(o["models"].isArray()&&o["models"].toArray().size()==1);const auto m=o["models"].toArray()[0].toObject(),c=o["context"].toObject();
 check(c["confirmed"].toBool()&&c["connection_confirmed"].toBool());
 instrument::TxCwCalibration result;result.profileSha=sha.toStdString();result.verificationSha=hash(proof).toStdString();result.setupId=c["setup_id"].toString().toStdString();result.referencePlane=c["reference_plane"].toString().toStdString();
 result.frequency=number(m,"frequency_hz");result.slope=number(m,"db_per_db");result.dbmAt2048=number(m,"dbm_at_2048_rms");result.minRms=number(m,"min_rms_counts");result.maxRms=number(m,"max_rms_counts");
 instrument::txCwRms(result,result.frequency,-40);return result;
}

inline instrument::TxCwCalibration loadTxCw4025gContext(const instrument::TxCwCalibration &base,const QString &path,const QString &verificationPath){
 auto check=[](bool ok){if(!ok)throw std::runtime_error("4025G CW context/evidence mismatch");};
 auto read=[](const QString &p){QFile f(p);if(!f.open(QIODevice::ReadOnly)||f.size()<=0||f.size()>256*1024)throw std::runtime_error("4025G CW context unavailable");return f.readAll();};
 const auto bytes=read(path),proof=read(verificationPath);
 auto hash=[](const QByteArray &b){return QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex().toStdString();};
 const auto doc=QJsonDocument::fromJson(bytes),verification=QJsonDocument::fromJson(proof);check(doc.isObject()&&verification.isObject());
 const auto c=doc.object(),e=verification.object();
 auto number=[&](const QJsonObject &o,const char *key){check(o[key].isDouble()&&std::isfinite(o[key].toDouble()));return o[key].toDouble();};
 check(base.contextSha.empty()&&c["kind"].toString()=="CW_REFERENCE_CONTEXT_OFFSET"&&e["kind"].toString()=="MANUAL_CW_CONTEXT_EVIDENCE");
 for(const auto &o:{c,e}){
  check(number(o,"schema")==1&&o["status"].toString()=="ONE_INDEPENDENT_CORRECTED_POINT_PASS");
  check(o["base_profile_sha256"].toString().toStdString()==base.profileSha&&number(o,"frequency_hz")==1.79e9);
  check(o["setup_id"].toString()=="zcu111_j7_4025g_current_cable_20261031"&&o["reference_plane"].toString()=="4025G_input_including_current_cable");
  check(o["absolute_accuracy_claimed"].isBool()&&!o["absolute_accuracy_claimed"].toBool());
 }
 check(c["verification_sha256"].toString().toStdString()==hash(proof)&&c["waveform"].toString()=="CW"&&c["extrapolation"].toString()=="FORBIDDEN");
 check(number(c,"minimum_dbm")==-40&&number(c,"maximum_dbm")==-20);
 const double offset=number(c,"command_offset_db");check(offset==number(e,"command_offset_db"));
 const auto points=e["baseline_points"].toArray();check(points.size()==4);
 auto mean=[&](const QJsonObject &p){const auto values=p["readings_dbm"].toArray();check(values.size()==3);double total=0;
  for(const auto &v:values){check(v.isDouble()&&std::isfinite(v.toDouble())&&v.toDouble()>-100&&v.toDouble()<0);total+=v.toDouble();}
  check(p["DAC_stopped_after_readings"].toString()=="USER_CONFIRMED");return total/3;};
 double sumDifference=0;const double targets[]={-40,-30,-20,-35};
 for(int i=0;i<4;++i){const auto p=points[i].toObject();check(number(p,"target_dbm")==targets[i]);const double observed=mean(p);
  if(i<3){check(p["role"].toString()=="screening_point");sumDifference+=observed-targets[i];}
  else check(p["excluded_from_correction_estimation"].toBool()&&p["role"].toString()=="predeclared_independent_check_point");}
 check(std::abs(offset-std::round(-sumDifference/3*100)/100)<1e-9);
 const auto independent=e["independent_corrected_check"].toObject();
 check(number(independent,"target_dbm")==-25&&std::abs(number(independent,"base_model_command_dbm")-(-25+offset))<1e-9);
 check(independent["role"].toString()=="new_independent_corrected_physical_check"&&independent["excluded_from_correction_estimation"].toBool());
 check(std::abs(mean(independent)+25)<.1&&std::abs(number(independent,"peak_frequency_hz")-1.79e9)<50000);
 for(const auto &v:independent["readings_dbm"].toArray())check(std::abs(v.toDouble()+25)<.1);
 const auto iqSha=independent["command_iq_sha256"].toString();check(iqSha.size()==64&&iqSha.toLatin1().count(' ')==0);
 auto result=base;result.commandOffsetDb=offset;result.minimumDbm=-40;result.maximumDbm=-20;result.contextSha=hash(bytes);result.contextVerificationSha=hash(proof);
 result.setupId=c["setup_id"].toString().toStdString();result.referencePlane=c["reference_plane"].toString().toStdString();
 instrument::txCwRms(result,result.frequency,-40);instrument::txCwRms(result,result.frequency,-20);return result;
}
