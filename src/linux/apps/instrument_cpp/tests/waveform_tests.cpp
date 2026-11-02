// Verify the generated signal property with an independent receiver-side matched filter.
#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace instrument;
namespace {
constexpr double pi=3.14159265358979323846;
double receiveTap(double t){
    constexpr double beta=.35;
    if(std::abs(t)<1e-12)return 1+beta*(4/pi-1);
    if(std::abs(std::abs(t)-1/(4*beta))<1e-10)return beta/std::sqrt(2.)*((1+2/pi)*std::sin(pi/(4*beta))+(1-2/pi)*std::cos(pi/(4*beta)));
    return (std::sin(pi*t*(1-beta))+4*beta*t*std::cos(pi*t*(1+beta)))/(pi*t*(1-std::pow(4*beta*t,2)));
}
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
double checkWave(const fs::path &file,bool qam){
    std::ifstream input(file,std::ios::binary);Bytes bytes(131072);input.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));require(bool(input),"Short generated file");
    std::vector<std::complex<double>> data(bytes.size()/4);
    for(std::size_t k=0;k<data.size();++k){auto component=[&](unsigned offset){return std::int16_t(std::uint16_t(bytes[k*4+offset])|(std::uint16_t(bytes[k*4+offset+1])<<8));};data[k]={double(component(0)),double(component(2))};}
    std::vector<std::complex<double>> symbols;
    for(std::size_t center=12*240;center+12*240<data.size();center+=240){std::complex<double> value{};for(int delta=-1440;delta<=1440;++delta)value+=data[std::size_t(std::int64_t(center)+delta)]*receiveTap(double(delta)/240);symbols.push_back(value);}
    double gain=0;for(const auto &z:symbols)gain+=qam?std::norm(z):std::abs(z);gain=qam?std::sqrt(gain/symbols.size()/42):gain/symbols.size();
    std::vector<std::complex<double>> ideal(symbols.size());
    if(qam){
        for(int iteration=0;iteration<8;++iteration){double numerator=0,denominator=0;for(std::size_t k=0;k<symbols.size();++k){auto level=[&](double v){return std::clamp(2*std::nearbyint((v/gain-1)/2)+1,-7.,7.);};ideal[k]={level(symbols[k].real()),level(symbols[k].imag())};numerator+=std::real(std::conj(ideal[k])*symbols[k]);denominator+=std::norm(ideal[k]);}gain=numerator/denominator;}
        for(auto &value:ideal)value*=gain;
    }else for(std::size_t k=0;k<symbols.size();++k)ideal[k]=std::polar(gain,std::nearbyint(std::arg(symbols[k])*8/(2*pi))*2*pi/8);
    double error=0,power=0;for(std::size_t k=0;k<symbols.size();++k){error+=std::norm(symbols[k]-ideal[k]);power+=std::norm(ideal[k]);}
    const auto evm=std::sqrt(error/power);require(evm<.02,"Receiver-side matched-filter constellation validation failed");return evm;
}
}
int main(int argc,char **argv){
    try{
        require(argc==2,"Need a test-output root");const fs::path base=argv[1];fs::create_directories(base);const auto root=base/("run_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));require(fs::create_directory(root),"Cannot create unique test output");std::atomic_bool cancel{false};
        {
            WaveRequest invalid;invalid.type="BARKER13";invalid.period=10e-6;invalid.output=root/"invalid_barker.bin";
            bool rejected=false;try{generateWaveform(invalid,cancel);}catch(const std::exception&){rejected=true;}
            require(rejected&&!fs::exists(invalid.output)&&!fs::exists(invalid.output.string()+".part"),"Truncated Barker code was published");
        }
        for(bool qam:{false,true}){WaveRequest request;request.type=qam?"QAM":"PSK";request.order=qam?64:8;request.offset=0;request.output=root/(qam?"qam64.bin":"psk8.bin");generateWaveform(request,cancel);const auto evm=checkWave(request.output,qam);std::cout<<request.type<<" DIGITAL_FILE_MATCHED_FILTER_EVM="<<evm<<" (not RF EVM)\n";}
        std::cout<<"WAVEFORM_CONTRACT_PASS C++_ONLY NO_RF_ACTION\n";return 0;
    }catch(const std::exception &e){std::cerr<<"WAVEFORM_CONTRACT_FAIL: "<<e.what()<<'\n';return 2;}
}
