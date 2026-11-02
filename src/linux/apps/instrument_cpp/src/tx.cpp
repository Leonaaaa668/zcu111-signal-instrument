#include "tx.hpp"
#include <cerrno>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <sstream>
#ifdef __linux__
#include "zcu111_tx_uapi.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif
namespace instrument {
namespace {
[[noreturn]] void error(const char *what){throw std::runtime_error(std::string(what)+": "+std::strerror(errno));}
#ifdef __linux__
TxInfo query(int fd){ztx_info raw{};if(ioctl(fd,ZTX_IOC_INFO,&raw)<0)error("Linux TX status");if(raw.abi_version!=1||raw.struct_bytes!=sizeof(raw)||raw.max_bytes!=ZTX_MAX_BYTES)throw std::runtime_error("Linux TX ABI/capability mismatch");return {raw.flags,raw.guard_status,raw.cdma_status,raw.mm2s_status,raw.dac_state,std::int32_t(raw.last_errno),raw.loaded_bytes,raw.received_bytes,raw.max_bytes,raw.session,raw.nco_word};}
#endif
}
bool txAvailable(){
#ifdef __linux__
 return access("/dev/zcu111_tx",R_OK|W_OK)==0;
#else
 return false;
#endif
}
TxInfo txInspect(){
#ifdef __linux__
 const int fd=open("/dev/zcu111_tx",O_RDONLY|O_CLOEXEC);if(fd<0)error("Open Linux TX status");try{auto i=query(fd);close(fd);return i;}catch(...){close(fd);throw;}
#else
 throw std::runtime_error("TX requires the Linux device backend");
#endif
}
std::string txInfoJson(const TxInfo&i){std::ostringstream o;o<<"{\"abi\":1,\"flags\":"<<i.flags<<",\"guard_status\":"<<i.guard<<",\"cdma_status\":"<<i.copyStatus<<",\"mm2s_status\":"<<i.playStatus<<",\"dac_state\":"<<i.dacState<<",\"last_errno\":"<<i.error<<",\"loaded_bytes\":"<<i.loaded<<",\"received_bytes\":"<<i.received<<",\"session\":"<<i.session<<",\"nco_word\":"<<i.nco<<"}\n";return o.str();}
TxSession::TxSession(){
#ifdef __linux__
 fd=open("/dev/zcu111_tx",O_RDWR|O_CLOEXEC);if(fd<0)error("Open Linux TX session");try{query(fd);}catch(...){close(fd);fd=-1;throw;}
#else
 throw std::runtime_error("Linux TX device unavailable on this platform");
#endif
}
TxSession::~TxSession(){
#ifdef __linux__
 if(fd>=0)close(fd); // Kernel release mutes and stops DMA even after a client failure.
#endif
}
TxInfo TxSession::info()const{
#ifdef __linux__
 return query(fd);
#else
 throw std::runtime_error("Linux TX backend required");
#endif
}
void TxSession::upload(const fs::path &file,const std::atomic_bool &cancel,std::function<void(unsigned,unsigned)> progress){
#ifdef __linux__
 auto bytes=readFile(file,64*1024*1024);
 if(bytes.empty()||bytes.size()>ZTX_MAX_BYTES||bytes.size()%256)throw std::runtime_error("TX IQ length must be <=64MiB and 256-byte aligned");
 const auto digest=sha256(bytes);ztx_begin begin{};begin.bytes=bytes.size();for(unsigned k=0;k<32;k++)begin.sha256[k]=std::uint8_t(std::stoul(digest.substr(k*2,2),nullptr,16));
 if(cancel.load())throw std::runtime_error("TX upload cancelled before transfer");
 if(ioctl(fd,ZTX_IOC_BEGIN,&begin)<0)error("Begin verified TX upload");
 try{
  std::size_t done=0;while(done<bytes.size()){
   if(cancel.load())throw std::runtime_error("TX upload cancelled; no playback");
   auto count=std::min<std::size_t>(4*1024*1024,bytes.size()-done);auto n=write(fd,bytes.data()+done,count);
   if(n<0)error("TX PS/PL DDR round-trip upload");
   if(std::size_t(n)!=count)throw std::runtime_error("Short TX upload; rejected");
   done+=count;if(progress)progress(unsigned(done),unsigned(bytes.size()));
  }
  if(cancel.load())throw std::runtime_error("TX upload cancelled before commit");
  if(ioctl(fd,ZTX_IOC_COMMIT)<0)error("TX digest/round-trip commit");
  auto i=info();if(!(i.flags&ZTX_F_LOADED)||i.loaded!=bytes.size()||(i.flags&ZTX_F_PLAYING))throw std::runtime_error("TX commit state mismatch");
 }catch(...){ioctl(fd,ZTX_IOC_ABORT);throw;}
#else
 (void)file;(void)cancel;(void)progress;throw std::runtime_error("Linux TX backend required");
#endif
}
void TxSession::play(){
#ifdef __linux__
 if(ioctl(fd,ZTX_IOC_PLAY)<0)error("TX play rejected");
#else
 throw std::runtime_error("Linux TX backend required");
#endif
}
void TxSession::stop(){
#ifdef __linux__
 if(ioctl(fd,ZTX_IOC_STOP)<0)error("TX stop failed");
#else
 throw std::runtime_error("Linux TX backend required");
#endif
}
void TxSession::prepare(){
#ifdef __linux__
 if(ioctl(fd,ZTX_IOC_PREPARED)<0)error("TX board preparation not ready");
#else
 throw std::runtime_error("Linux TX backend required");
#endif
}
void TxSession::diagnose(){
#ifdef __linux__
 ztx_dma_diag diagnostic{};
 if(ioctl(fd,ZTX_IOC_DIAG,&diagnostic)<0)error("Read-only DMA diagnostic");
 if(diagnostic.cdma_status&0x770U||diagnostic.mm2s_status&0x770U)throw std::runtime_error("DMA diagnostic reports hardware error; inspect kernel log");
#else
 throw std::runtime_error("Linux TX backend required");
#endif
}

}
