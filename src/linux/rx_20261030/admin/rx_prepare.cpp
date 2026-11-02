// SPDX-License-Identifier: MIT
// Release the known result block's DMA reset through an exclusive UIO mapping.
// No /dev/mem, DMA register access, capture trigger or RF playback.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "zcu111_tx_uapi.h"
namespace {void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}}
int main(int argc,char **argv){try{
 check(geteuid()==0&&argc==2,"Usage: root zcu111-rx-prepare /dev/uioN");
 const std::string path=argv[1];check(path.rfind("/dev/uio",0)==0&&path.find('/',5)==std::string::npos,"Expected a UIO device path");const auto name=path.substr(5);
 std::uint64_t address=0,size=0;std::ifstream a("/sys/class/uio/"+name+"/maps/map0/addr"),b("/sys/class/uio/"+name+"/maps/map0/size");a>>std::hex>>address;b>>std::hex>>size;check(address==0xb00d0000ULL&&size==0x1000,"Wrong UIO result register range");
 int tx=open("/dev/zcu111_tx",O_RDONLY|O_CLOEXEC);check(tx>=0,"TX initialization must own the control clock");ztx_info info{};check(ioctl(tx,ZTX_IOC_INFO,&info)==0&&info.abi_version==1&&(info.flags&ZTX_F_PREPARED)&&!(info.flags&ZTX_F_PLAYING),"TX not initialized/stopped");
 int fd=open(path.c_str(),O_RDWR|O_CLOEXEC);check(fd>=0,"Cannot open result UIO");void *map=mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);check(map!=MAP_FAILED,"Cannot map result UIO");auto *reg=static_cast<volatile std::uint32_t*>(map);std::cout<<"RESULT_CONTROL="<<std::hex<<reg[0]<<" STATUS="<<reg[1]<<" ID="<<reg[2]<<" LAST_RESPONSE="<<reg[3]<<std::dec<<'\n'<<std::flush;check(reg[2]==0x31535852U,"Result IP ID mismatch");
 // CANCEL=1 releases HOLD_RESET=0 without arming a packet. The source is idle.
 reg[0]=2;std::atomic_thread_fence(std::memory_order_seq_cst);(void)reg[0];
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
 while(reg[1]&0x0f02U){check(std::chrono::steady_clock::now()<deadline,"Result cancel/idle timeout");std::this_thread::sleep_for(std::chrono::microseconds(50));}
 reg[0]=0;std::atomic_thread_fence(std::memory_order_seq_cst);check(reg[0]==0,"HOLD_RESET/CANCEL did not clear");
 munmap(map,size);close(fd);close(tx);std::cout<<"RX_HOLD_RESET_RELEASED_NO_CAPTURE_NO_PLAY\n";return 0;
}catch(const std::exception&e){std::cerr<<"RX_PREPARE_ERROR: "<<e.what()<<'\n';return 2;}}
