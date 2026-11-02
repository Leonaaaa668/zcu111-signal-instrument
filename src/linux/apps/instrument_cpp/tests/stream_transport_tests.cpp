#include "core.hpp"
#include "latest.hpp"
#include "zcu111_sa_uapi.h"
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
using namespace instrument;
namespace {
constexpr int fake=9876;unsigned opens=0,closes=0,configs=0,receipts=0;bool shortRead=false,stale=false;
Bytes original,active;std::size_t position=0;Receipt current;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
void put(unsigned index,std::uint32_t value){for(unsigned b=0;b<4;++b)active[index*4+b]=std::uint8_t(value>>(8*b));}
}
extern "C" int __real_open(const char*,int,...);
extern "C" int __real_close(int);
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" int __real_access(const char*,int);
extern "C" int __wrap_access(const char *name,int mode){if(std::strcmp(name,"/dev/zcu111_spectrum")==0)return 0;return __real_access(name,mode);}
// rxAvailable also uses stat(), covered independently by the board test. Mock it
// as a char device so host tests exercise the same public live path.
#include <sys/stat.h>
extern "C" int __real_stat(const char*,struct stat*);
extern "C" int __wrap_stat(const char *name,struct stat *out){if(std::strcmp(name,"/dev/zcu111_spectrum")==0){std::memset(out,0,sizeof(*out));out->st_mode=S_IFCHR|0660;return 0;}return __real_stat(name,out);}
extern "C" int __wrap_open(const char *name,int flags,...){
    if(std::strcmp(name,"/dev/zcu111_spectrum")==0){++opens;return fake;}
    if(flags&O_CREAT){va_list v;va_start(v,flags);auto mode=va_arg(v,int);va_end(v);return __real_open(name,flags,mode);}
    return __real_open(name,flags);
}
extern "C" int __wrap_close(int fd){if(fd==fake){++closes;return 0;}return __real_close(fd);}
extern "C" ssize_t __wrap_read(int fd,void *out,size_t count){
    if(fd!=fake)return __real_read(fd,out,count);
    if(shortRead)return 0;
    // Force partial reads, including receipt ownership until packet completion.
    auto n=std::min({count,active.size()-position,std::size_t(777)});
    std::memcpy(out,active.data()+position,n);position+=n;return ssize_t(n);
}
extern "C" int __wrap_ioctl(int fd,unsigned long command,...){
    if(fd!=fake){errno=EBADF;return -1;}va_list v;va_start(v,command);auto *arg=va_arg(v,void*);va_end(v);
    if(command==ZSA_IOC_CONFIG){
        auto *cfg=static_cast<zsa_config*>(arg);check(cfg->log_n==10,"Wrong stream FFT configuration");
        ++configs;active=original;position=0;const auto serial=stale?1:configs;
        current.ticket=serial;current.epoch=serial;current.sequence=serial;current.counterEpoch=serial;
        put(5,serial);put(6,0);put(7,serial);put(10,serial);put(30,serial);put(31,0);return 0;
    }
    if(command==ZSA_IOC_RECEIPT){++receipts;check(position==active.size(),"Receipt requested before full frame");
        auto *r=static_cast<zsa_receipt*>(arg);*r={current.ticket,current.epoch,current.segment,current.ncoSeq,current.logn,current.zoom,current.sequence,current.counterEpoch};return 0;}
    errno=ENOTTY;return -1;
}
int main(int argc,char **argv){try{
    check(argc==2,"Need resource root");const fs::path root=argv[1];auto resources=loadResources(root);
    original=readFile(root/"fixtures/lower/spectrum.bin");current=loadReceipt(root/"fixtures/lower/spectrum.bin.receipt.json");
    CaptureRequest request;request.n=1024;request.frames=2;std::atomic_bool cancel{false};unsigned callbacks=0;
    streamSpectra(request,resources,cancel,[&](CaptureResult &&r,std::uint64_t batch){
        check(r.hasData&&r.measurement.frames==2&&r.lastSequence==2*batch,"Stream delivered wrong/partial batch");
        check(r.output.empty()&&r.archiveMs<10,"Stream unexpectedly archived");
        if(++callbacks==4)cancel.store(true);
    },0);
    check(opens==1&&closes==1&&configs==8&&receipts==8&&callbacks==4,"Receiver not retained for complete stream");
    streamSpectra(request,resources,cancel,[](CaptureResult&&,std::uint64_t){throw std::runtime_error("Cancelled stream published");},0);
    check(opens==1,"Pre-cancelled stream opened hardware");
    cancel=false;shortRead=true;bool failed=false;
    try{streamSpectra(request,resources,cancel,[](CaptureResult&&,std::uint64_t){},0);}catch(const std::exception&){failed=true;}
    check(failed&&opens==2&&closes==2,"Read error did not close stream device");
    shortRead=false;stale=true;request.frames=1;failed=false;
    try{streamSpectra(request,resources,cancel,[](CaptureResult&&,std::uint64_t){},0);}catch(const std::exception&){failed=true;}
    check(failed&&opens==3&&closes==3,"Repeated stale stream batch was accepted");
    LatestFrame<int> slot;slot.publish(1);slot.publish(2);auto last=slot.take();
    check(last&&*last==2&&slot.replaced()==1&&!slot.take(),"Latest mailbox retained stale frames");slot.clear();
    std::atomic_bool done{false};std::thread producer([&]{for(int n=1;n<=10000;++n)slot.publish(n);done=true;});
    int consumed=0,value=0;while(!done){auto x=slot.take();if(x){check(*x>value,"Mailbox reordered data");value=*x;++consumed;}}
    producer.join();auto x=slot.take();if(x){value=*x;++consumed;}
    check(value==10000&&slot.replaced()+consumed==10000,"Mailbox grew/lost accounting");
    std::cout<<"STREAM_CONTRACT_PASS persistent FD, partial reads, cancellation, stale rejection, bounded mailbox\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
