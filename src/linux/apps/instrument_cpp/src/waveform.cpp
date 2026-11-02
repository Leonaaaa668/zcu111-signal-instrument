#include "core.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace instrument {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
double rrc(double t,double beta) {
    if(std::abs(t)>6)return 0;
    if(std::abs(t)<1e-12)return 1+beta*(4/pi-1);
    if(std::abs(std::abs(4*beta*t)-1)<1e-10)
        return beta/std::sqrt(2.)*((1+2/pi)*std::sin(pi/(4*beta))+(1-2/pi)*std::cos(pi/(4*beta)));
    return (std::sin(pi*t*(1-beta))+4*beta*t*std::cos(pi*t*(1+beta)))/(pi*t*(1-16*beta*beta*t*t));
}
}
void generateWaveform(const WaveRequest &r,std::atomic_bool &cancel,std::function<void(unsigned,unsigned)> progress) {
    check(r.sampleRate==1.2e9&&r.duration==.010,"Released TX format is 1.2GS/s complex, exactly 10ms");
    check(r.peak>=1&&r.peak<=32700,"IQ component peak must be 1..32700 counts");
    check(!fs::exists(r.output)&&!fs::exists(r.output.string()+".part")&&!fs::exists(r.output.string()+".json")&&!fs::exists(r.output.string()+".json.part"),"Waveform output already exists");
    check(fs::space(r.output.parent_path().empty()?fs::path("."):r.output.parent_path()).available>=48000000+4096,"Insufficient reported capacity for a 10ms IQ record; choose /tmp or SD storage, not the zero-capacity initramfs root");
    check(std::isfinite(r.offset)&&std::abs(r.offset)<=500e6,"IQ offset outside intended RF band");
    const bool qam=r.type=="QAM",psk=r.type=="PSK",pulse=r.type=="PULSE",lfm=r.type=="LFM",lfmPulse=r.type=="LFM_PULSE",barker=r.type=="BARKER13",cw=r.type=="CW";
    double modelRms=0;
    if(r.powerModel){check(cw&&r.offset==10e6,"dBm power mode requires1790MHz CW");modelRms=txCwRms(*r.powerModel,1.8e9-r.offset,r.targetDbm);}
    check(qam||psk||pulse||lfm||lfmPulse||barker||cw,"Unknown waveform type");
    constexpr std::uint64_t count=12000000;
    check(std::isfinite(r.period)&&std::isfinite(r.pulseWidth)&&r.period>=1e-6&&r.period<=.01&&r.pulseWidth>0&&r.pulseWidth<=r.period,"Invalid pulse timing");
    const auto period=std::uint64_t(std::llround(r.period*r.sampleRate));
    const auto width=std::uint64_t(std::llround(r.pulseWidth*r.sampleRate));
    check(std::isfinite(r.period)&&std::isfinite(r.pulseWidth)&&r.period>=1e-6&&r.period<=.01&&r.pulseWidth>0&&r.pulseWidth<=r.period&&period>0&&width>0&&count%period==0,"Pulse timing must form a periodic 10ms record");
    if(barker)check(period>=13*1200,"Barker13 needs a period >=13us for all 13 chips");
    check(std::isfinite(r.bandwidth)&&r.bandwidth>0&&r.bandwidth<=40e6,"LFM bandwidth must be >0 and <=40MHz");
    check(std::abs(r.offset*r.duration-std::round(r.offset*r.duration))<1e-7,"IQ offset must be coherent with the cyclic 10ms record");
    unsigned sps=240;std::vector<std::complex<double>> symbols;std::vector<std::array<double,13>> phases;
    if(qam||psk) {
        check(std::isfinite(r.symbolRate)&&r.symbolRate>=1e5&&r.symbolRate<=10e6&&std::isfinite(r.rolloff)&&r.rolloff>0&&r.rolloff<=1,"Invalid symbol rate/rolloff");
        const double ratio=r.sampleRate/r.symbolRate;sps=unsigned(std::llround(ratio));
        check(sps>=120&&sps<=12000&&std::abs(ratio-sps)<1e-8&&count%sps==0,"Need integral samples/symbol and a periodic record");
        if(qam)check(r.order==16||r.order==64||r.order==256,"QAM order must be 16/64/256");
        else check(r.order==2||r.order==4||r.order==8,"PSK order must be 2/4/8");
        std::mt19937 generator(r.seed);symbols.resize(count/sps);const unsigned side=unsigned(std::sqrt(double(r.order)));
        for(auto &symbol:symbols) {
            if(qam) {
                const int i=int(generator()%side)*2-int(side-1),q=int(generator()%side)*2-int(side-1);
                symbol={double(i),double(q)};
            } else {const auto phase=2*pi*(generator()%r.order)/r.order;symbol=std::polar(1.,phase);}
        }
        phases.resize(sps);
        // symbols[pos-k] is a delay of k symbols: h(n/T - (pos-k)) = h(phase/T+k).
        for(unsigned phase=0;phase<sps;++phase)for(int k=-6;k<=6;++k)phases[phase][k+6]=rrc(double(phase)/sps+k,r.rolloff);
    }
    auto sample=[&](std::uint64_t n)->std::complex<double> {
        if(qam||psk) {
            const auto pos=n/sps;const auto phase=n%sps;std::complex<double> value{};
            for(int k=-6;k<=6;++k) {
                const auto index=(static_cast<std::int64_t>(pos)-k+static_cast<std::int64_t>(symbols.size()))%static_cast<std::int64_t>(symbols.size());
                value+=symbols[std::size_t(index)]*phases[std::size_t(phase)][std::size_t(k+6)];
            }
            return value*std::polar(1.,2*pi*r.offset*double(n)/r.sampleRate);
        }
        const auto pos=n%period;const double t=double(pos)/r.sampleRate;
        double amplitude=1,phase=2*pi*r.offset*double(n)/r.sampleRate;
        if(pulse||lfmPulse) {
            amplitude=pos<width?1:0;const double edge=std::min(100e-9,r.pulseWidth/8);
            if(t<edge)amplitude=.5-.5*std::cos(pi*t/edge);
            if(t>=r.pulseWidth-edge&&t<r.pulseWidth)amplitude=.5-.5*std::cos(pi*(r.pulseWidth-t)/edge);
        }
        if(lfm||lfmPulse) {
            const double duration=lfmPulse?r.pulseWidth:r.period;
            phase+=2*pi*(-r.bandwidth*.5*t+r.bandwidth*t*t/(2*duration));
        }
        if(barker) {
            static const int code[13]={1,1,1,1,1,-1,-1,1,1,-1,1,-1,1};
            const auto chip=std::uint64_t(std::llround(1e-6*r.sampleRate));
            amplitude=pos<13*chip?code[pos/chip]:0;
        }
        return std::complex<double>(std::cos(phase),std::sin(phase))*amplitude;
    };
    double maximum=0;
    for(std::uint64_t n=0;n<count;++n) {
        if((n&16383)==0) {check(!cancel.load(),"Waveform generation cancelled before publication");if(progress)progress(unsigned(n),unsigned(count*2));}
        const auto value=sample(n);maximum=std::max({maximum,std::abs(value.real()),std::abs(value.imag())});
    }
    check(maximum>0&&std::isfinite(maximum),"Invalid waveform scale");
    const double scale=r.powerModel?modelRms:r.peak/maximum;
    check(scale*maximum<=32700,"Requested power exceeds DAC waveform peak headroom");
    const fs::path partial=r.output.string()+".part";
    ExclusiveOutput stream(partial);
    std::vector<char> buffer;buffer.reserve(65536);long double energy=0;unsigned actualPeak=0;
    for(std::uint64_t n=0;n<count;++n) {
        if((n&16383)==0) {check(!cancel.load(),"Waveform cancelled; incomplete .part retained");if(progress)progress(unsigned(count+n),unsigned(count*2));}
        const auto value=sample(n)*scale;const auto i=std::int16_t(std::nearbyint(value.real())),q=std::int16_t(std::nearbyint(value.imag()));
        energy+=double(i)*i+double(q)*q;
        actualPeak=std::max({actualPeak,unsigned(std::abs(int(i))),unsigned(std::abs(int(q)))});
        for(auto v:{i,q}) {const auto u=std::uint16_t(v);buffer.push_back(char(u&255));buffer.push_back(char(u>>8));}
        if(buffer.size()>=65536) {stream.write(buffer.data(),buffer.size());buffer.clear();}
    }
    if(!buffer.empty())stream.write(buffer.data(),buffer.size());
    stream.finish();
    check(fs::file_size(partial)==count*4,"Incomplete generated binary");
    check(!cancel.load(),"Waveform cancelled before publication; .part retained");
    const auto digest=sha256(readFile(partial,64*1024*1024));
    std::ostringstream json;json<<std::setprecision(17)<<"{\"kind\":\"GENERATED_IQ_NOT_PLAYING\",\"type\":\""<<r.type<<"\",\"bytes\":"<<count*4
        <<",\"sample_rate_hz\":"<<r.sampleRate<<",\"duration_s\":"<<r.duration<<",\"component_peak_counts\":"<<actualPeak
        <<",\"rms_complex_counts\":"<<std::sqrt(double(energy/count))<<",\"offset_hz\":"<<r.offset<<",\"symbol_rate_hz\":"<<((qam||psk)?r.symbolRate:0)
        <<",\"modulation_order\":"<<((qam||psk)?r.order:0)<<",\"rrc_rolloff\":"<<((qam||psk)?r.rolloff:0)<<",\"pulse_width_s\":"<<(barker?13e-6:r.pulseWidth)<<",\"pulse_period_s\":"<<r.period
        <<",\"chirp_bandwidth_hz\":"<<r.bandwidth<<",\"seed\":"<<r.seed<<",\"sha256\":\""<<digest<<"\""
        <<",\"layout\":\"little endian int16 I0 Q0 I1 Q1\",\"rf_convention\":\"RF approximately 1.8GHz minus IQ frequency; preserve measured DAC orientation\",\"hardware_actions\":\"NONE\",\"rf_board_validation\":\"PENDING_FOR_THIS_GENERATOR\"";
    if(r.powerModel){const auto &m=*r.powerModel;const double predicted=txCwPowerDbm(m,std::sqrt(double(energy/count)));
        check(std::abs(predicted-r.targetDbm)<=.1,"Quantized waveform misses requested model power");
        json<<",\"power_control\":\"CW_MODEL_DBM\",\"requested_dbm\":"<<r.targetDbm<<",\"predicted_cw_dbm\":"<<predicted<<",\"tx_power_profile_sha256\":\""<<m.profileSha<<"\",\"tx_power_verification_sha256\":\""<<m.verificationSha<<"\",\"power_reference_plane\":\""<<m.referencePlane<<"\",\"power_setup_id\":\""<<m.setupId<<"\"";
        json<<",\"base_model_command_dbm\":"<<r.targetDbm+m.commandOffsetDb<<",\"context_command_offset_db\":"<<m.commandOffsetDb;
        if(!m.contextSha.empty())json<<",\"tx_context_profile_sha256\":\""<<m.contextSha<<"\",\"tx_context_verification_sha256\":\""<<m.contextVerificationSha<<"\",\"context_validation_scope\":\"One independent corrected CW point; full-range absolute accuracy not certified\"";
    }else json<<",\"power_control\":\"DIGITAL_UNCALIBRATED\"";
    json<<"}\n";
    auto text=json.str();const fs::path metadata=r.output.string()+".json.part";
    writeFile(metadata,Bytes(text.begin(),text.end()));check(!cancel.load(),"Waveform cancelled before publication; incomplete parts retained");
    publishNew(partial,r.output);publishNew(metadata,r.output.string()+".json");
    if(progress)progress(100,100);
}
}
