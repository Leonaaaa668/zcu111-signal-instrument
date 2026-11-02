#pragma once
#include "core.hpp"
#include "rx_power.hpp"
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <cmath>
#include <stdexcept>
inline instrument::RxCwCalibration loadRxCwCalibration(const QString &path,const instrument::Resources &resources){
    QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("no RX CW calibration profile loaded");
    if(f.size()<=0||f.size()>128*1024)throw std::runtime_error("invalid RX calibration file size");
    const auto bytes=f.readAll();QJsonParseError parse;const auto document=QJsonDocument::fromJson(bytes,&parse);
    if(parse.error!=QJsonParseError::NoError||!document.isObject())throw std::runtime_error("invalid RX calibration JSON");
    const auto o=document.object();
    auto expect=[&](bool ok){if(!ok)throw std::runtime_error("RX calibration status/context mismatch");};
    auto number=[&](const char *key){const auto v=o.value(key);expect(v.isDouble()&&std::isfinite(v.toDouble()));return v.toDouble();};
    const auto version=o.value("format_version");const auto kind=o.value("kind").toString();
    expect(version.isDouble()&&((version.toDouble()==1&&kind=="RX_CW_ENERGY_5_BIN_NOISE_SUBTRACTED")||
        (version.toDouble()==2&&kind=="RX_CW_ENERGY_5_BIN_NOISE_SUBTRACTED__AFFINE_DB")));
    expect(o.value("status").toString()=="MEASURED_PROJECT_SCREENING_PASS"&&o.value("reference_plane").toString()=="XM500_J1_INPUT");
    expect(o.value("channel").toString()=="ADC_TILE1_ADC1"&&o.value("nco_word").toString()=="400000000000");
    expect(o.value("absolute_accuracy_claimed_db").isNull()&&number("independent_holdout_points")>=1&&number("training_points")>=3);
    const QByteArray fingerprint(reinterpret_cast<const char*>(resources.fingerprint.data()),int(resources.fingerprint.size()));
    expect(o.value("window_fingerprint_hex").toString()==QString::fromLatin1(fingerprint.toHex()));
    instrument::RxCwCalibration c;c.id=o.value("id").toString().toStdString();c.reference=o.value("reference_instrument").toString().toStdString();
    expect(!c.id.empty()&&!c.reference.empty());
    expect(number("fft_n")==65536);c.n=65536;c.fs=number("sample_rate_hz");c.center=number("center_hz");c.frequency=number("frequency_hz");
    c.offsetDb=number("offset_db");c.minimumDbm=number("minimum_input_dbm");c.maximumDbm=number("maximum_input_dbm");
    if(version.toDouble()==2){expect(o.value("holdout_used_for_fit").isBool()&&!o.value("holdout_used_for_fit").toBool());c.gainDbPerDb=number("gain_db_per_db");c.minimumFrames=16;}
    expect(c.gainDbPerDb>=.95&&c.gainDbPerDb<=1.05);
    c.noiseWindowFs=number("noise_window_linear_fs");c.minSnrDb=number("minimum_local_snr_db");c.validationLimitDb=number("screening_limit_db");
    c.trainResidualDb=number("training_max_residual_db");c.holdoutResidualDb=number("holdout_max_error_db");
    expect(c.fs==150e6&&c.center==1.8e9&&c.frequency==1.79e9&&std::abs(c.offsetDb)<200&&c.minimumDbm<c.maximumDbm&&c.minimumDbm>=-120&&c.maximumDbm<=10);
    expect(c.noiseWindowFs>0&&c.noiseWindowFs<1&&c.minSnrDb>=25&&c.minSnrDb<=100&&c.validationLimitDb>0&&c.validationLimitDb<=.1&&c.trainResidualDb>=0&&c.trainResidualDb<=c.validationLimitDb&&c.holdoutResidualDb>=0&&c.holdoutResidualDb<=c.validationLimitDb);
    c.sha256=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().toStdString();return c;
}
