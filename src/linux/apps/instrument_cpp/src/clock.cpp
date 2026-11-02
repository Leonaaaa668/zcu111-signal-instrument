#include "clock.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#endif
namespace instrument {
namespace {
constexpr const char *tableHash="c996f2dce13a78cd66092f109cedcffc21bd8568d6a7f62a444208c67cdbee50";
void check(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
#ifdef __linux__
fs::path adapterRoot(){return fs::exists("/sys/class/i2c-adapter")?fs::path("/sys/class/i2c-adapter"):fs::path("/sys/bus/i2c/devices");}
#endif
std::string hex(const Bytes &bytes){std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:bytes)out<<std::setw(2)<<unsigned(b);return out.str();}
}
std::vector<ClockStep> clockPlan(const fs::path &directory){
    const auto raw=readFile(directory/"clock_tables.h");check(sha256(raw)==tableHash,"Clock-table fingerprint changed; programming forbidden");
    const std::string text(raw.begin(),raw.end());
    auto table=[&](const std::string &name,std::size_t count){
        std::smatch match;check(std::regex_search(text,match,std::regex("\\b"+name+"\\[[0-9]+\\]\\s*=\\s*\\{([^}]+)\\}")),"Missing clock table");
        const auto body=match[1].str();std::regex pattern("0x([0-9a-fA-F]+)u");std::vector<std::uint32_t> values;
        for(auto i=std::sregex_iterator(body.begin(),body.end(),pattern);i!=std::sregex_iterator();++i)values.push_back(std::uint32_t(std::stoul((*i)[1].str(),nullptr,16)));
        check(values.size()==count,"Clock table length mismatch");return values;
    };
    const auto lmk=table("adc3_lmk_12288",26),lmx=table("adc3_lmx_2400",113);
    std::vector<ClockStep> steps={{{0xf0,0x02},25,"SC18IS602 config"}};
    auto spi=[&](unsigned select,std::uint32_t value,unsigned length,const std::string &kind){Bytes bytes{std::uint8_t(select)};for(int k=int(length)-1;k>=0;--k)bytes.push_back(std::uint8_t(value>>(8*k)));steps.push_back({bytes,25,kind});};
    for(auto value:lmk)spi(2,value,4,"LMK");
    steps.push_back({{},2000,"settle"});
    for(unsigned select:{8u,1u}){const std::string name=select==8?"ADC U102":"DAC U104";spi(select,lmx[112]|2,3,name);spi(select,lmx[112]&~2u,3,name);for(auto value:lmx)spi(select,value,3,name);steps.push_back({{},10,"settle"});spi(select,lmx[112],3,name);steps.push_back({{},2000,"settle"});}
    return steps;
}
std::string clockPlanJson(const std::vector<ClockStep> &steps){
    std::ostringstream out;out<<std::setprecision(17)<<"{\"clock_tables_sha256\":\""<<tableHash<<"\",\"topology\":\"kernel mux channel5 child\",\"steps\":[";
    for(std::size_t i=0;i<steps.size();++i){if(i)out<<',';const auto &s=steps[i];out<<'{';if(!s.data.empty())out<<"\"i2c_address\":47,\"data_hex\":\""<<hex(s.data)<<"\",";out<<"\"delay_seconds\":"<<s.delayMs/1000.<<",\"kind\":\""<<s.kind<<"\"}";}
    out<<"]}\n";return out.str();
}
void replayClockPlan(const std::vector<ClockStep> &steps,const ClockTransport &transport){
    check(bool(transport.write)&&bool(transport.sleep),"Incomplete clock transport");
    for(const auto &step:steps){if(!step.data.empty())check(transport.write(step.data)==step.data.size(),"Short clock I2C write; clock initialization failed");transport.sleep(step.delayMs);}
}
std::string listClockAdapters(){
    std::ostringstream out;
#ifdef __linux__
    const fs::path root=adapterRoot();
    if(!fs::exists(root))return "No Linux I2C adapter sysfs directory\n";
    for(const auto &entry:fs::directory_iterator(root)){
        if(!std::regex_match(entry.path().filename().string(),std::regex("i2c-[0-9]+")))continue;
        std::ifstream file(entry.path()/"name");std::string name;std::getline(file,name);out<<entry.path().filename().string()<<" : "<<name;
        std::error_code error;const auto mux=fs::canonical(entry.path()/"mux_device",error);
        if(!error){out<<" ; mux="<<mux.string();for(unsigned channel=0;channel<8;++channel){const auto link=fs::canonical(mux/("channel-"+std::to_string(channel)),error);if(!error&&link==fs::canonical(entry.path()))out<<" ; channel="<<channel;}}
        out<<'\n';
    }
#else
    out<<"Adapter discovery is available on Linux only; no hardware access\n";
#endif
    return out.str();
}
void applyClockPlan(const fs::path &adapter,const std::vector<ClockStep> &steps){
#ifdef __linux__
    const auto device=adapter.string();check(std::regex_match(device,std::regex("/dev/i2c-[0-9]+")),"Select an explicit /dev/i2c-N child adapter");
    const auto name=adapter.filename();const auto child=fs::canonical(adapterRoot()/name);
    const auto mux=fs::canonical(child/"mux_device");
    check(child.string().find("ff030000")!=std::string::npos,"Adapter is not under PS I2C1");
    check(std::regex_match(mux.filename().string(),std::regex("[0-9]+-0074")),"Adapter is not behind the expected 0x74 board mux");
    check(fs::canonical(mux/"channel-5")==child,"Clock programming requires mux channel5 child");
    struct Fd{int value=-1;~Fd(){if(value>=0)close(value);}} fd;fd.value=open(device.c_str(),O_RDWR|O_CLOEXEC);check(fd.value>=0,"Cannot open clock I2C adapter");
    check(ioctl(fd.value,I2C_SLAVE,0x2f)==0,"SPI bridge cannot be claimed; kernel ownership must be resolved without FORCE");
    ClockTransport transport;
    transport.write=[&](const Bytes &data){const auto count=::write(fd.value,data.data(),data.size());check(count>=0,"Clock I2C write failed");return std::size_t(count);};
    transport.sleep=[](unsigned ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));};replayClockPlan(steps,transport);
#else
    (void)adapter;(void)steps;throw std::runtime_error("Clock apply requires Linux");
#endif
}
}
