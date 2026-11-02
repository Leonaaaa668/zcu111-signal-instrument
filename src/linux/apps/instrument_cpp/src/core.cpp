#include "core.hpp"
#include "storage.hpp"
#include "tuning.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <chrono>
#include <thread>
#ifdef __linux__
#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "zcu111_sa_uapi.h"
#endif

namespace instrument {
namespace {
void require(bool condition, const char *message) { if(!condition) throw std::runtime_error(message); }
std::uint32_t u32(const Bytes &data, std::size_t offset) {
    require(offset+4<=data.size(),"Short integer");
    return std::uint32_t(data[offset])|(std::uint32_t(data[offset+1])<<8)|
           (std::uint32_t(data[offset+2])<<16)|(std::uint32_t(data[offset+3])<<24);
}
std::uint64_t pair(const std::array<std::uint32_t,64> &h,unsigned k) {return h[k]|(std::uint64_t(h[k+1])<<32);}
double db(double value) {return value>0 ? 10*std::log10(value) : -300.;}
std::string escape(const std::string &text) {
    std::string output;
    for(unsigned char c:text) {
        if(c=='"'||c=='\\') output+='\\';
        if(c<32) {std::ostringstream s;s<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c);output+=s.str();}
        else output+=char(c);
    }
    return output;
}
std::uint32_t rotate(std::uint32_t x,unsigned n) {return (x>>n)|(x<<(32-n));}
}

Bytes readFile(const fs::path &path,std::size_t limit) {
    const auto size=fs::file_size(path);
    require(size<=limit,"File exceeds size limit");
    std::ifstream stream(path,std::ios::binary);
    require(bool(stream),"Cannot open file");
    Bytes data(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(size));
    require(bool(stream)||size==0,"Short file read");
    return data;
}
void writeFile(const fs::path &path,const Bytes &data) {
    ExclusiveOutput stream(path);stream.write(data.data(),data.size());stream.finish();
}

std::string sha256(const Bytes &input) {
    static constexpr std::uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,8> state={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    Bytes data=input;const auto bits=std::uint64_t(data.size())*8;
    data.push_back(128);while(data.size()%64!=56)data.push_back(0);
    for(int i=7;i>=0;--i)data.push_back(std::uint8_t(bits>>(i*8)));
    for(std::size_t block=0;block<data.size();block+=64) {
        std::uint32_t w[64]{};
        for(unsigned i=0;i<16;++i)for(unsigned j=0;j<4;++j)w[i]=(w[i]<<8)|data[block+i*4+j];
        for(unsigned i=16;i<64;++i) {
            const auto s0=rotate(w[i-15],7)^rotate(w[i-15],18)^(w[i-15]>>3);
            const auto s1=rotate(w[i-2],17)^rotate(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        auto a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for(unsigned i=0;i<64;++i) {
            const auto t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
            const auto t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
    }
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto word:state)out<<std::setw(8)<<word;
    return out.str();
}

Resources loadResources(const fs::path &root) {
    Resources r;std::ifstream file(root/"window_metrics.tsv");
    unsigned n;WindowMetrics window;
    while(file>>n>>window.sum>>window.squareSum)require(r.windows.emplace(n,window).second,"Duplicate window entry");
    require(r.windows.size()==3&&r.windows.count(1024)&&r.windows.count(8192)&&r.windows.count(65536),"Missing window metrics");
    const auto digest=sha256(readFile(root/"hann_q17.mem",2*1024*1024));
    for(unsigned i=0;i<32;++i)r.fingerprint[i]=std::uint8_t(std::stoul(digest.substr(i*2,2),nullptr,16));
    return r;
}

Receipt loadReceipt(const fs::path &path) {
    auto bytes=readFile(path,4096);std::string text(bytes.begin(),bytes.end());std::size_t pos=0;
    if(text.size()>=3&&static_cast<unsigned char>(text[0])==0xef)pos=3;
    auto space=[&]{while(pos<text.size()&&std::isspace(static_cast<unsigned char>(text[pos])))++pos;};
    auto expect=[&](char c){space();require(pos<text.size()&&text[pos++]==c,"Invalid receipt JSON");};
    std::map<std::string,std::uint64_t> fields;expect('{');
    for(unsigned i=0;i<8;++i) {
        if(i)expect(',');
        expect('"');const auto begin=pos;
        while(pos<text.size()&&text[pos]!='"') {require(text[pos]!='\\',"Escaped receipt key unsupported");++pos;}
        require(pos<text.size(),"Missing receipt key terminator");const auto key=text.substr(begin,pos-begin);++pos;expect(':');space();
        const auto start=pos;while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;
        require(pos>start,"Receipt must contain unsigned integer values");
        require(pos-start==1||text[start]!='0',"Invalid receipt integer");
        require(fields.emplace(key,std::stoull(text.substr(start,pos-start))).second,"Duplicate receipt key");
    }
    expect('}');space();require(pos==text.size(),"Trailing receipt content");
    auto narrow=[&](const char *name){require(fields.count(name)!=0,"Missing receipt key");auto v=fields.at(name);require(v<=0xffffffffULL,"Receipt integer overflow");return std::uint32_t(v);};
    Receipt r;r.ticket=narrow("ticket_id");r.epoch=narrow("epoch");r.segment=narrow("segment_id");
    r.ncoSeq=narrow("nco_update_seq");r.logn=narrow("log_n");r.zoom=narrow("zoom");
    require(fields.count("packet_sequence")&&fields.count("counter_epoch"),"Missing receipt counters");
    r.sequence=fields.at("packet_sequence");r.counterEpoch=fields.at("counter_epoch");return r;
}
std::string receiptJson(const Receipt &r) {
    std::ostringstream s;s<<"{\"ticket_id\":"<<r.ticket<<",\"epoch\":"<<r.epoch<<",\"segment_id\":"<<r.segment
    <<",\"nco_update_seq\":"<<r.ncoSeq<<",\"log_n\":"<<r.logn<<",\"zoom\":"<<r.zoom
    <<",\"packet_sequence\":"<<r.sequence<<",\"counter_epoch\":"<<r.counterEpoch<<"}\n";return s.str();
}

Packet decode(Bytes data,const Receipt &receipt,const Resources &resources) {
    require(data.size()>=256,"Short packet header");Packet p;p.receipt=receipt;
    for(unsigned k=0;k<64;++k)p.header[k]=u32(data,k*4);
    const auto &h=p.header;const auto n=h[11],logn=h[12];
    require(h[0]==0x31504153&&h[1]==1&&h[2]==256&&h[3]==data.size()&&h[4]==0x3f,"Packet magic/version/length/flags mismatch");
    require(resources.windows.count(n)&&logn<32&&n==(1u<<logn)&&h[13]==0&&h[14]==logn+1,"Unreleased FFT geometry");
    require(data.size()==256+8ULL*n,"FFT packet length mismatch");
    require(pair(h,5)&&h[9]&&h[10]&&rxWordMatches(pair(h,20),pair(h,22)),"Invalid sequence/ticket/center/NCO");
    require(h[15]==1&&h[16]==2400000000u&&h[17]==1200000000u&&h[18]==150000000&&h[19]==1200000000u,"Sample-rate/sign mismatch");
    require(h[23]<=0xffff&&h[32]==0&&h[37]==1&&h[38]==0&&h[39]==0,"Bad snapshot quality");
    const std::array<std::uint32_t,6> units={17,8,24,16,1,0};
    for(unsigned k=0;k<6;++k)require(h[44+k]==units[k]&&h[50+k]==0,"Invalid units/reserved fields");
    const auto &window=resources.windows.at(n);
    require(pair(h,40)==window.sum&&pair(h,42)==window.squareSum,"Window normalization mismatch");
    require(std::equal(resources.fingerprint.begin(),resources.fingerprint.end(),data.begin()+224),"Window fingerprint mismatch");
    require((h[36]&31)==15&&!(h[36]&0x7fe01fe0)&&((h[36]>>13)&31)==logn&&((h[36]>>18)&7)==0,"DSP state mismatch");
    require(pair(h,28)>=pair(h,26)&&(pair(h,28)-pair(h,26))%4==0,"Invalid observation timebase");
    require(receipt.ticket==h[10]&&receipt.epoch==h[7]&&receipt.segment==h[8]&&receipt.ncoSeq==h[9]&&receipt.logn==logn&&receipt.zoom==0&&receipt.sequence==pair(h,5)&&receipt.counterEpoch==pair(h,30),"Stale/mixed capture receipt");
    std::uint64_t checksum=0;p.power.resize(n);
    for(unsigned k=0;k<n;++k) {
        auto v=std::uint64_t(u32(data,256+k*8))|(std::uint64_t(u32(data,260+k*8))<<32);
        require(v<=(1ULL<<47),"Power range exceeded");p.power[k]=v;checksum^=v;
    }
    require(checksum==pair(h,34),"Payload XOR64 mismatch");
    p.hash=sha256(data);p.n=n;p.bytes=std::move(data);return p;
}

namespace {
void finalizeMeasurement(Measurement &m){
    m.toneValid=false;m.toneHz=0;
    m.peak=unsigned(std::max_element(m.average.begin(),m.average.end())-m.average.begin());
    const auto signedBin=m.peak<m.n/2?int(m.peak):int(m.peak)-int(m.n);
    m.peakHz=m.center+signedBin*m.spacing;m.peakDbfs=db(m.coherent[m.peak]);m.totalDbfs=db(std::accumulate(m.energy.begin(),m.energy.end(),0.));
    long double mainlobe=0;for(int k=-2;k<=2;++k)mainlobe+=m.energy[(int(m.peak)+k+int(m.n))%int(m.n)];m.mainlobeDbfs=db(double(mainlobe));
    auto sorted=m.average;std::nth_element(sorted.begin(),sorted.begin()+m.n/2,sorted.end());
    const double a=m.average[(m.peak+m.n-1)%m.n],b=m.average[m.peak],c=m.average[(m.peak+1)%m.n];
    if(std::min({a,b,c})>0&&b>sorted[m.n/2]*10) {
        const auto denominator=std::log(a)-2*std::log(b)+std::log(c);
        if(denominator<0) {
            const auto delta=.5*(std::log(a)-std::log(c))/denominator;
            if(std::abs(delta)<=.5) {m.toneValid=true;m.toneHz=m.center+(std::fmod(m.peak+delta+m.n/2.,double(m.n))-m.n/2.)*m.spacing;}
        }
    }
    for(unsigned k=0;k<m.n;++k)m.displayDbfs[k]=db(m.coherent[(k+m.n/2)%m.n]);
}
}

Measurement measure(const std::vector<Packet> &packets) {
    require(!packets.empty(),"No frames");Measurement m;const auto &first=packets.front();const auto &h=first.header;
    m.n=first.n;m.frames=unsigned(packets.size());m.ncoSequence=h[9];m.ncoWord=pair(h,22);m.fs=h[18];m.center=double(pair(h,20));m.spacing=m.fs/m.n;
    m.average.assign(m.n,0);std::set<std::string> hashes;
    std::vector<long double> sum(m.n,0);
    for(const auto &p:packets) {
        require(p.n==first.n,"Cannot average different FFT sizes");
        for(unsigned k: {9u,14u,15u,18u,20u,21u,22u,23u,40u,41u,42u,43u,44u,45u,46u,47u,48u,49u})require(p.header[k]==h[k],"Cannot average different geometry/window/NCO");
        require(hashes.insert(p.hash).second,"Duplicate packet cannot count as another average");
        m.hashes.push_back(p.hash);for(unsigned k=0;k<m.n;++k)sum[k]+=p.power[k];
    }
    const double sw=double(pair(h,40)),sw2=double(pair(h,42));
    m.enbwBins=m.n*sw2/(sw*sw);m.enbwHz=m.fs*sw2/(sw*sw);
    const auto scale=std::pow(std::ldexp(1.,int(h[14]))/(sw/std::ldexp(1.,17))/std::ldexp(1.,23),2);
    m.coherent.resize(m.n);m.energy.resize(m.n);m.displayDbfs.resize(m.n);
    for(unsigned k=0;k<m.n;++k) {
        m.average[k]=double(sum[k]/packets.size());m.coherent[k]=m.average[k]*scale;m.energy[k]=m.coherent[k]/m.enbwBins;
    }
    finalizeMeasurement(m);
    return m;
}

void mergeMeasurements(Measurement &m,const Measurement &b){
    if(!m.frames){m=b;return;}
    require(m.n==b.n&&m.fs==b.fs&&m.center==b.center&&m.enbwBins==b.enbwBins&&m.ncoSequence==b.ncoSequence&&m.ncoWord==b.ncoWord,"Cannot merge different FFT/window/NCO acquisitions");
    require(b.frames&&m.frames<=std::numeric_limits<unsigned>::max()-b.frames,"Invalid accumulated frame count");
    require(m.average.size()==m.n&&b.average.size()==m.n&&m.coherent.size()==m.n&&b.coherent.size()==m.n&&m.energy.size()==m.n&&b.energy.size()==m.n,"Incomplete spectra in dwell");
    std::set<std::string> hashes(m.hashes.begin(),m.hashes.end());
    for(const auto &hash:b.hashes)require(hashes.insert(hash).second,"Repeated frame in sweep dwell");
    const double count=double(m.frames)+b.frames;
    for(unsigned k=0;k<m.n;++k){
        m.average[k]=(m.average[k]*m.frames+b.average[k]*b.frames)/count;
        m.coherent[k]=(m.coherent[k]*m.frames+b.coherent[k]*b.frames)/count;
        m.energy[k]=(m.energy[k]*m.frames+b.energy[k]*b.frames)/count;
    }
    m.frames+=b.frames;m.hashes.insert(m.hashes.end(),b.hashes.begin(),b.hashes.end());
    finalizeMeasurement(m);
}

std::string measurementJson(const Measurement &m) {
    std::ostringstream s;s<<std::setprecision(17)<<"{\n\"status\":\"VALIDATED_SPECTRUM_MEASUREMENTS\",\n\"fft_n\":"<<m.n<<",\"frames\":"<<m.frames
    <<",\"fs_complex_hz\":"<<m.fs<<",\"center_hz\":"<<m.center<<",\"bin_spacing_hz\":"<<m.spacing<<",\"window_enbw_bins\":"<<m.enbwBins
    <<",\"window_enbw_hz\":"<<m.enbwHz<<",\"peak_bin\":"<<m.peak<<",\"peak_bin_rf_hz\":"<<m.peakHz<<",\"interpolated_tone_rf_hz\":";
    if(m.toneValid)s<<m.toneHz;else s<<"null";
    auto power=[&](const char *key,double value){s<<",\""<<key<<"\":";if(value<=-300)s<<"null";else s<<value;};
    power("peak_coherent_bin_dbfs",m.peakDbfs);power("mainlobe_five_bin_dbfs",m.mainlobeDbfs);power("total_window_corrected_dbfs",m.totalDbfs);
    s<<",\"receiver_dbm_calibrated\":false,\"receiver_dbm\":null,\"packet_sha256\":[";
    for(std::size_t i=0;i<m.hashes.size();++i){if(i)s<<',';s<<'"'<<escape(m.hashes[i])<<'"';}
    s<<"],\"frequency_estimator\":\"Three-bin log parabola; not certified accuracy\",\"averaging\":\"Linear-power mean before logarithm\"}\n";return s.str();
}
void saveMeasurement(const fs::path &directory,const Measurement &m) {
    require(fs::create_directory(directory),"Measurement output already exists");
    auto json=measurementJson(m);writeFile(directory/"measurement.json",Bytes(json.begin(),json.end()));
    std::ofstream csv(directory/"spectrum.csv");csv<<std::setprecision(17)<<"bin,offset_hz,rf_hz,mean_raw_power,coherent_bin_dbfs,energy_bin_dbfs,psd_dbfs_per_hz\n";
    for(unsigned k=0;k<m.n;++k) {
        const auto bin=(k+m.n/2)%m.n;const double offset=(int(k)-int(m.n/2))*m.spacing;
        csv<<bin<<','<<offset<<','<<m.center+offset<<','<<m.average[bin]<<',';
        if(m.coherent[bin]>0)csv<<db(m.coherent[bin]);
        csv<<',';
        if(m.energy[bin]>0)csv<<db(m.energy[bin]);
        csv<<',';
        if(m.energy[bin]>0)csv<<db(m.energy[bin]/m.spacing);
        csv<<'\n';
    }
    csv.close();require(bool(csv),"CSV write failed");
}
bool rxAvailable() {
#ifdef __linux__
    struct stat status{};return stat("/dev/zcu111_spectrum",&status)==0&&S_ISCHR(status.st_mode)&&access("/dev/zcu111_spectrum",R_OK)==0;
#else
    return false;
#endif
}
static CaptureResult captureImpl(const CaptureRequest &request,const Resources &resources,std::atomic_bool &cancel,std::function<void(unsigned,unsigned)> progress,int sharedFd=-1,const RxTuning *sharedTuning=nullptr) {
    using Clock=std::chrono::steady_clock;
    const auto ms=[](Clock::time_point begin){return std::chrono::duration<double,std::milli>(Clock::now()-begin).count();};
    require(resources.windows.count(request.n),"Unreleased FFT size");require(request.frames>=1&&request.frames<=256,"Frames must be 1..256");
    require(request.live||request.replay.size()==request.frames,"Replay needs one independent packet per frame");
    require(!request.live||rxAvailable(),"Linux spectrum character device is unavailable");
    if(request.archive)require(fs::create_directory(request.output),"Capture directory exists or parent is missing");
    CaptureResult result;result.live=request.live;result.output=request.output;std::vector<Packet> packets;
#ifdef __linux__
    RxTuning tuning;
    if(request.live)tuning=sharedTuning?*sharedTuning:tuneReceiver(request.centerHz);
    struct Fd {int value=-1;~Fd(){if(value>=0)close(value);}} fd;
    if(request.live&&sharedFd<0) {fd.value=open("/dev/zcu111_spectrum",O_RDONLY|O_CLOEXEC);require(fd.value>=0,"Cannot open RX device");}
    const int receiveFd=sharedFd>=0?sharedFd:fd.value;
#endif
    for(unsigned index=0;index<request.frames&&!cancel.load();++index) {
        const auto readStart=Clock::now();Bytes data;Receipt receipt;
        if(!request.live) {data=readFile(request.replay[index]);receipt=loadReceipt(request.replay[index].string()+".receipt.json");}
        else {
#ifdef __linux__
            unsigned logn=0;while((1u<<logn)<request.n)++logn;zsa_config config{logn,0};
            if(tuning.legacy)require(ioctl(receiveFd,ZSA_IOC_CONFIG,&config)==0,"RX CONFIG ioctl failed; reinitialize failed backend before retry");
            else {zsa_config_v2 cfg{logn,tuning.sequence,request.centerHz};require(ioctl(receiveFd,ZSA_IOC_CONFIG_V2,&cfg)==0,"RX tune generation changed; frame rejected");}
            data.resize(256+8ULL*request.n);std::size_t position=0;
            while(position<data.size()) {
                const auto got=read(receiveFd,data.data()+position,data.size()-position);
                if(got<0&&errno==EINTR)continue;
                require(got>0,"RX read failed or returned short packet; no automatic DMA reset");position+=std::size_t(got);
            }
            zsa_receipt raw{};require(ioctl(receiveFd,ZSA_IOC_RECEIPT,&raw)==0,"RX RECEIPT ioctl failed");
            receipt={raw.ticket_id,raw.epoch,raw.segment_id,raw.nco_update_seq,raw.log_n,raw.zoom,raw.packet_sequence,raw.counter_epoch};
#else
            throw std::runtime_error("Live capture requires Linux");
#endif
        }
        result.readMs+=ms(readStart);const auto validateStart=Clock::now();
        auto packet=decode(std::move(data),receipt,resources);require(packet.n==request.n,"Packet FFT size differs from request");
        if(request.live){require(pair(packet.header,20)==request.centerHz,"RX frame center differs from requested center");
#ifdef __linux__
            require(tuning.legacy||packet.header[9]==tuning.sequence,"RX frame belongs to another tune generation");
#endif
        }
        if(request.live&&result.lastSequence)require(receipt.sequence>result.lastSequence&&receipt.counterEpoch>result.lastCounterEpoch,"Live frame sequence/time did not advance");
        if(!result.firstSequence)result.firstSequence=receipt.sequence;
        result.lastSequence=receipt.sequence;result.lastCounterEpoch=receipt.counterEpoch;
        result.validateMs+=ms(validateStart);
        const auto archiveStart=Clock::now();
        if(request.archive){
        std::ostringstream name;name<<"frame_"<<std::setw(4)<<std::setfill('0')<<index<<".bin";
        writeFile(request.output/name.str(),packet.bytes);auto json=receiptJson(receipt);
        writeFile(request.output/(name.str()+".receipt.json"),Bytes(json.begin(),json.end()));
        }
        result.archiveMs+=ms(archiveStart);packets.push_back(std::move(packet));if(progress)progress(index+1,request.frames);
    }
    result.cancelled=cancel.load();result.hasData=!packets.empty();
    if(result.hasData) {const auto begin=Clock::now();result.measurement=measure(packets);result.measureMs=ms(begin);}
    const auto archiveStart=Clock::now();if(request.archive){
    if(result.hasData)saveMeasurement(request.output/"measurement",result.measurement);
    std::string state=std::string("{\"source\":\"")+(result.live?"live":"replay")+"\",\"cancelled\":"+(result.cancelled?"true":"false")+",\"frames\":"+std::to_string(packets.size())+"}\n";
    writeFile(request.output/"acquisition.json",Bytes(state.begin(),state.end()));}
    result.archiveMs+=ms(archiveStart);return result;
}
CaptureResult capture(const CaptureRequest &request,const Resources &resources,std::atomic_bool &cancel,std::function<void(unsigned,unsigned)> progress) {
    return captureImpl(request,resources,cancel,std::move(progress));
}
void streamSpectra(CaptureRequest request,const Resources &resources,std::atomic_bool &cancel,
                   const std::function<void(CaptureResult &&,std::uint64_t)> &publish,unsigned periodMs) {
    require(request.live,"Streaming requires the live Linux RX device");
    require(resources.windows.count(request.n)&&request.frames>=1&&request.frames<=256,"Invalid stream geometry");
    require(bool(publish)&&periodMs<=1000,"Invalid stream callback/cadence");
    if(cancel.load())return;
    request.archive=false;request.output.clear();
#ifdef __linux__
    const auto tuning=tuneReceiver(request.centerHz);
    struct Handle {int fd=-1;~Handle(){if(fd>=0)close(fd);}} handle;
    handle.fd=open("/dev/zcu111_spectrum",O_RDONLY|O_CLOEXEC);
    require(handle.fd>=0,"Cannot open RX stream device");
    std::uint64_t batches=0,previousSequence=0,previousEpoch=0;
    while(!cancel.load()) {
        const auto begin=std::chrono::steady_clock::now();
        auto result=captureImpl(request,resources,cancel,{},handle.fd,&tuning);
        // A cancelled partial averaging batch is never published as a full batch.
        if(cancel.load()||result.cancelled)break;
        require(result.hasData&&result.measurement.frames==request.frames,"Incomplete stream batch");
        require(!previousSequence||(result.firstSequence>previousSequence&&result.lastCounterEpoch>previousEpoch),"Stale stream batch");
        previousSequence=result.lastSequence;previousEpoch=result.lastCounterEpoch;
        publish(std::move(result),++batches);
        // Leave CPU time for the compositor. No UI wait and no archive I/O.
        const auto deadline=begin+std::chrono::milliseconds(periodMs);
        while(!cancel.load()&&std::chrono::steady_clock::now()<deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
#else
    throw std::runtime_error("Live streaming requires Linux");
#endif
}

}
