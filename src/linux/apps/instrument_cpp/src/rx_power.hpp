#pragma once
#include <string>
namespace instrument {
struct Measurement;
struct RxCwCalibration {
    std::string id,sha256,reference;
    unsigned n=65536,minimumFrames=1;
    double fs=150e6,center=1.8e9,frequency=1.79e9,offsetDb=0,gainDbPerDb=1;
    double minimumDbm=-60,maximumDbm=-30,noiseWindowFs=0,minSnrDb=25;
    double validationLimitDb=.1,trainResidualDb=0,holdoutResidualDb=0;
};
struct RxCwReading {
    bool valid=false;std::string reason;
    double dbm=0,signalDbfs=0,snrDb=0,concentration=0;
};
RxCwReading rxCwPower(const Measurement &m,const RxCwCalibration &cal);
}
