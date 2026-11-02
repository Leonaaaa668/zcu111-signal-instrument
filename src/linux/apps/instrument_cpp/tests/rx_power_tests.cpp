#include "core.hpp"
#include "rx_power.hpp"
#include <cmath>
#include <limits>
#include <iostream>
#include <stdexcept>
using namespace instrument;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    RxCwCalibration c;c.offsetDb=12;c.minimumDbm=-61;c.maximumDbm=-30;c.noiseWindowFs=3e-11;c.trainResidualDb=.03;c.holdoutResidualDb=.04;
    Measurement m;m.n=65536;m.frames=16;m.fs=150e6;m.center=1.8e9;m.spacing=m.fs/m.n;m.peak=61167;m.peakHz=m.center-4369*m.spacing;
    const auto fill=[&](double inputDbm){m.energy.assign(m.n,c.noiseWindowFs/5);const auto power=std::pow(10.,((inputDbm-c.offsetDb)/c.gainDbPerDb)/10);const double weight[]={.02,.23,.5,.23,.02};for(int i=0;i<5;++i)m.energy[m.peak-2+i]+=power*weight[i];};
    fill(-40);auto r=rxCwPower(m,c);check(r.valid&&std::abs(r.dbm+40)<1e-10,"energy/noise/CW conversion");
    c.gainDbPerDb=1.01268;fill(-45);r=rxCwPower(m,c);check(r.valid&&std::abs(r.dbm+45)<1e-10,"affine dB conversion");
    auto invalidGain=c;invalidGain.gainDbPerDb=1.2;check(!rxCwPower(m,invalidGain).valid,"excessive gain accepted");
    c.gainDbPerDb=1;fill(-40);
    auto averaged=c;averaged.minimumFrames=16;m.frames=1;check(!rxCwPower(m,averaged).valid,"unvalidated one-frame power accepted");m.frames=16;
    auto bad=c;bad.offsetDb=std::numeric_limits<double>::quiet_NaN();check(!rxCwPower(m,bad).valid,"NaN calibration accepted");
    bad=c;bad.holdoutResidualDb=.11;check(!rxCwPower(m,bad).valid,"failed independent validation accepted");
    m.n=8192;check(!rxCwPower(m,c).valid,"wrong FFT accepted");m.n=65536;
    m.center=1.81e9;check(!rxCwPower(m,c).valid,"wrong center accepted");m.center=1.8e9;
    m.peakHz=1.81e9;check(!rxCwPower(m,c).valid,"wrong tone accepted");m.peakHz=m.center-4369*m.spacing;
    fill(-20);check(!rxCwPower(m,c).valid,"power extrapolation accepted");
    fill(-90);check(!rxCwPower(m,c).valid,"noise-floor signal accepted");
    fill(-40);for(int i=-16;i<=16;++i)m.energy[(int(m.peak)+i+int(m.n))%int(m.n)]=1e-6;check(!rxCwPower(m,c).valid,"wideband signal labelled CW");
    fill(-40);m.energy[m.peak]=std::numeric_limits<double>::quiet_NaN();check(!rxCwPower(m,c).valid,"NaN data accepted");
    std::cout<<"RX_POWER_TESTS_PASS energy scale, noise subtraction, scope guards, holdout guard and CW concentration\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
