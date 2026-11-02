// User-space transaction tests with a simulated device transport. No MMIO/RF.
#include "tx.hpp"
#include "zcu111_tx_uapi.h"
#include <cstdarg>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace instrument;
namespace {
constexpr int device=910;
struct Mock {
 unsigned begins=0,commits=0,aborts=0,plays=0,closed=0,writes=0;
 bool shortWrite=false,badAbi=false,badCommit=false;
 std::uint32_t flags=ZTX_F_PREPARED;
 std::uint64_t bytes=0,received=0;
} mock;
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
template<class F> void rejects(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}require(rejected,"Fault was accepted");}
}
extern "C" int __real_open(const char*,int,...);
extern "C" int __real_close(int);
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" int __real_access(const char*,int);
extern "C" int __wrap_open(const char *name,int flags,...){
 if(!std::strcmp(name,"/dev/zcu111_tx"))return device;
 if(flags&O_CREAT){va_list ap;va_start(ap,flags);auto mode=va_arg(ap,int);va_end(ap);return __real_open(name,flags,mode);}
 return __real_open(name,flags);
}
extern "C" int __wrap_close(int fd){if(fd==device){++mock.closed;return 0;}return __real_close(fd);}
extern "C" int __wrap_access(const char *name,int flags){if(!std::strcmp(name,"/dev/zcu111_tx"))return 0;return __real_access(name,flags);}
extern "C" ssize_t __wrap_write(int fd,const void *data,size_t bytes){
 if(fd!=device)return __real_write(fd,data,bytes);
 ++mock.writes;
 const auto *p=static_cast<const unsigned char*>(data);
 for(unsigned k=0;k<bytes;++k)require(p[k]==static_cast<unsigned char>(k),"File bytes changed in transit");
 if(mock.shortWrite)return ssize_t(bytes/2);
 mock.received+=bytes;return ssize_t(bytes);
}
extern "C" int __wrap_ioctl(int fd,unsigned long request,...){
 require(fd==device,"Unexpected descriptor");
 if(request==ZTX_IOC_INFO){
  va_list ap;va_start(ap,request);auto *i=va_arg(ap,ztx_info*);va_end(ap);*i={};
  i->abi_version=mock.badAbi?99:1;i->struct_bytes=sizeof(*i);i->max_bytes=ZTX_MAX_BYTES;
  i->flags=mock.flags;i->loaded_bytes=(mock.flags&ZTX_F_LOADED)?mock.bytes:0;i->received_bytes=mock.received;return 0;
 }
 if(request==ZTX_IOC_BEGIN){
  va_list ap;va_start(ap,request);auto *b=va_arg(ap,ztx_begin*);va_end(ap);
  constexpr unsigned char digest[]={0x40,0xaf,0xf2,0xe9,0xd2,0xd8,0x92,0x2e,0x47,0xaf,0xd4,0x64,0x8e,0x69,0x67,0x49,0x71,0x58,0x78,0x5f,0xbd,0x1d,0xa8,0x70,0xe7,0x11,0x02,0x66,0xbf,0x94,0x48,0x80};
  require(b->bytes==256&&!std::memcmp(b->sha256,digest,32),"BEGIN length/SHA256 differ from independent fixture");
  ++mock.begins;mock.bytes=b->bytes;return 0;
 }
 if(request==ZTX_IOC_COMMIT){++mock.commits;if(!mock.badCommit)mock.flags|=ZTX_F_LOADED;return 0;}
 if(request==ZTX_IOC_ABORT){++mock.aborts;mock.flags=ZTX_F_PREPARED;return 0;}
 if(request==ZTX_IOC_PLAY){++mock.plays;return 0;}
 if(request==ZTX_IOC_STOP)return 0;
 throw std::runtime_error("Unexpected ioctl");
}
int main(int argc,char **argv){try{
 require(argc==2,"Need evidence directory");fs::path root=fs::path(argv[1])/("run_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
 auto file=root/"aligned.bin";{std::ofstream stream(file,std::ios::binary);for(unsigned k=0;k<256;++k)stream.put(char(k));}
 std::atomic_bool cancel{false};
 mock={};{auto info=txInspect();require(info.flags==ZTX_F_PREPARED,"Status read failed");}require(!mock.begins&&!mock.plays&&mock.closed==1,"Read-only inspection changed transmit session");
 mock={};mock.badAbi=true;rejects([]{TxSession s;});require(mock.closed==1&&!mock.begins,"ABI mismatch leaked descriptor or began upload");
 mock={};{TxSession s;s.upload(file,cancel);}require(mock.begins==1&&mock.writes==1&&mock.commits==1&&!mock.aborts&&!mock.plays,"Upload silently started playback or omitted commit");
 mock={};mock.shortWrite=true;{TxSession s;rejects([&]{s.upload(file,cancel);});}require(mock.aborts==1&&!mock.commits&&!mock.plays,"Short write did not abort");
 mock={};cancel.store(true);{TxSession s;rejects([&]{s.upload(file,cancel);});}require(!mock.begins&&!mock.plays,"Pre-cancelled request reached device");cancel.store(false);
 mock={};{TxSession s;rejects([&]{s.upload(file,cancel,[&](unsigned,unsigned){cancel.store(true);});});}require(mock.begins==1&&mock.aborts==1&&!mock.commits&&!mock.plays,"Cancellation before commit did not abort");cancel.store(false);
 mock={};mock.badCommit=true;{TxSession s;rejects([&]{s.upload(file,cancel);});}require(mock.aborts==1&&!mock.plays,"Invalid commit state was accepted");
 auto bad=root/"unaligned.bin";{std::ofstream stream(bad,std::ios::binary);stream.put('x');}
 mock={};{TxSession s;rejects([&]{s.upload(bad,cancel);});}require(!mock.begins&&!mock.plays,"Unaligned file reached device");
 std::cout<<"TX_USERSPACE_TRANSACTION_PASS checks=8 SIMULATED_DEVICE_NO_MMIO_NO_RF\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 2;}}
