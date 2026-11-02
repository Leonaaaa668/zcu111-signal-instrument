#include "rx_power.hpp"
#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace instrument {
RxCwReading rxCwPower(const Measurement &m,const RxCwCalibration &c){
    RxCwReading r;
    auto reject=[&](const char *why){r.reason=why;return r;};
    if(c.n!=65536||c.fs!=150e6||c.center!=1.8e9||c.frequency!=1.79e9||
       !std::isfinite(c.offsetDb)||std::abs(c.offsetDb)>200||
       !std::isfinite(c.gainDbPerDb)||c.gainDbPerDb<.95||c.gainDbPerDb>1.05||
       !std::isfinite(c.noiseWindowFs)||c.noiseWindowFs<=0||c.noiseWindowFs>=1||
       !std::isfinite(c.minimumDbm)||!std::isfinite(c.maximumDbm)||c.minimumDbm>=c.maximumDbm||
       !std::isfinite(c.minSnrDb)||c.minSnrDb<25||c.minSnrDb>100||
       !std::isfinite(c.validationLimitDb)||c.validationLimitDb<=0||c.validationLimitDb>.1||
       !std::isfinite(c.trainResidualDb)||c.trainResidualDb<0||c.trainResidualDb>c.validationLimitDb||
       !std::isfinite(c.holdoutResidualDb)||c.holdoutResidualDb<0||c.holdoutResidualDb>c.validationLimitDb)
        return reject("invalid/unqualified calibration profile");
    if(m.n!=c.n||m.fs!=c.fs||m.center!=c.center||m.energy.size()!=m.n||m.frames==0)
        return reject("FFT/rate/center differs from calibration");
    if(m.frames<c.minimumFrames)return reject("set Linear average >= 16 for calibrated CW power");
    const double spacing=m.fs/m.n;
    if(m.peak>=m.n||!std::isfinite(m.peakHz)||std::abs(m.peakHz-c.frequency)>3*spacing)
        return reject("no dominant CW near 1790 MHz");
    const auto bin=static_cast<int>(std::llround((c.frequency-c.center)/spacing));
    double window=0,outside=0;
    for(int delta=-16;delta<=16;++delta){const auto index=unsigned((bin+delta+int(m.n))%int(m.n));const double value=m.energy[index];
        if(!std::isfinite(value)||value<0)return reject("invalid energy data");
        if(std::abs(delta)<=2)window+=value;else outside+=value;}
    const double signal=window-c.noiseWindowFs;
    if(signal<=0)return reject("below calibration noise baseline");
    r.signalDbfs=10*std::log10(signal);r.snrDb=10*std::log10(signal/c.noiseWindowFs);
    if(r.snrDb<c.minSnrDb)return reject("insufficient local CW signal/noise");
    const double outsideSignal=std::max(0.,outside-c.noiseWindowFs*28/5);
    r.concentration=signal/(signal+outsideSignal);
    if(r.concentration<.8)return reject("signal is not a narrow CW in this window");
    r.dbm=c.gainDbPerDb*r.signalDbfs+c.offsetDb;
    if(!std::isfinite(r.dbm)||r.dbm<c.minimumDbm-c.validationLimitDb||r.dbm>c.maximumDbm+c.validationLimitDb)
        return reject("outside measured input-power range");
    r.valid=true;r.reason="CW input estimate / project reference";return r;
}
}
