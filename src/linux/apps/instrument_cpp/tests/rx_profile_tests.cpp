#include "rx_power_qt.hpp"
#include <QtCore/QTemporaryDir>
#include <iostream>
#include <limits>

// Synthetic profiles test parsing only; they are never installed as calibration.
int main(int argc,char **argv){try{
    if(argc!=2)throw std::runtime_error("resource directory required");
    const auto resources=instrument::loadResources(argv[1]);
    QTemporaryDir temporary;if(!temporary.isValid())throw std::runtime_error("temporary directory unavailable");
    const auto path=temporary.filePath("synthetic_test_only.json");
    const QByteArray fingerprint(reinterpret_cast<const char*>(resources.fingerprint.data()),int(resources.fingerprint.size()));
    QJsonObject valid{{"format_version",1},{"id","SYNTHETIC_TEST_ONLY"},{"reference_instrument","synthetic"},
        {"kind","RX_CW_ENERGY_5_BIN_NOISE_SUBTRACTED"},{"status","MEASURED_PROJECT_SCREENING_PASS"},
        {"reference_plane","XM500_J1_INPUT"},{"channel","ADC_TILE1_ADC1"},{"nco_word","400000000000"},
        {"absolute_accuracy_claimed_db",QJsonValue::Null},{"independent_holdout_points",1},{"training_points",4},
        {"window_fingerprint_hex",QString::fromLatin1(fingerprint.toHex())},{"fft_n",65536},{"sample_rate_hz",150e6},
        {"center_hz",1.8e9},{"frequency_hz",1.79e9},{"offset_db",12.0},{"minimum_input_dbm",-61.0},
        {"maximum_input_dbm",-31.0},{"noise_window_linear_fs",3e-11},{"minimum_local_snr_db",25.0},
        {"screening_limit_db",.1},{"training_max_residual_db",.03},{"holdout_max_error_db",.04}};
    auto write=[&](const QByteArray &bytes){QFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size())throw std::runtime_error("cannot write test profile");};
    auto reject=[&](const QJsonObject &object){write(QJsonDocument(object).toJson());bool rejected=false;try{loadRxCwCalibration(path,resources);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("unqualified profile accepted");};
    write(QJsonDocument(valid).toJson());const auto c=loadRxCwCalibration(path,resources);
    if(c.id!="SYNTHETIC_TEST_ONLY"||c.sha256.size()!=64||c.offsetDb!=12)throw std::runtime_error("valid profile/hash parsing failed");
    auto affine=valid;affine["format_version"]=2;affine["kind"]="RX_CW_ENERGY_5_BIN_NOISE_SUBTRACTED__AFFINE_DB";affine["gain_db_per_db"]=1.01268;affine["holdout_used_for_fit"]=false;
    write(QJsonDocument(affine).toJson());if(loadRxCwCalibration(path,resources).gainDbPerDb!=1.01268)throw std::runtime_error("affine profile gain lost");
    auto broken=affine;broken.remove("gain_db_per_db");reject(broken);
    broken=affine;broken["holdout_used_for_fit"]=true;reject(broken);
    broken=affine;broken["gain_db_per_db"]=1.2;reject(broken);
    for(const auto &key:{"offset_db","noise_window_linear_fs","training_points","independent_holdout_points","absolute_accuracy_claimed_db"}){auto bad=valid;bad.remove(key);reject(bad);}
    for(const auto &key:{"offset_db","fft_n","holdout_max_error_db"}){auto bad=valid;bad[key]="invalid numeric value";reject(bad);}
    auto bad=valid;bad["status"]="SINGLE_POINT_MEASURED_NOT_RANGE_VALIDATED";reject(bad);
    bad=valid;bad["training_points"]=1;reject(bad);
    bad=valid;bad["independent_holdout_points"]=0;reject(bad);
    bad=valid;bad["holdout_max_error_db"]=.1001;reject(bad);
    bad=valid;bad["absolute_accuracy_claimed_db"]=.1;reject(bad);
    bad=valid;bad["window_fingerprint_hex"]="wrong";reject(bad);
    bad=valid;bad["fft_n"]=1e100;reject(bad);
    bad=valid;bad["frequency_hz"]=1.8e9;reject(bad);
    write("{ malformed JSON");bool rejected=false;try{loadRxCwCalibration(path,resources);}catch(const std::exception&){rejected=true;}
    if(!rejected)throw std::runtime_error("malformed JSON accepted");
    std::cout<<"RX_PROFILE_TESTS_PASS context, fingerprint, independent validation, missing fields, numeric types and malformed JSON\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
