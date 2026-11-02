#include "storage.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif
namespace instrument {
namespace {void problem(const char *message){throw std::runtime_error(std::string(message)+": "+std::strerror(errno));}}
ExclusiveOutput::ExclusiveOutput(const fs::path &path){
#ifdef _WIN32
    descriptor=_wopen(path.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
#else
    descriptor=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0666);
#endif
    if(descriptor<0)problem("Cannot exclusively create output; existing files are preserved");
}
ExclusiveOutput::~ExclusiveOutput(){
    if(descriptor>=0){
#ifdef _WIN32
        _close(descriptor);
#else
        close(descriptor);
#endif
    }
}
void ExclusiveOutput::write(const void *raw,std::size_t length){
    auto *bytes=static_cast<const char*>(raw);
    while(length){
#ifdef _WIN32
        const auto count=_write(descriptor,bytes,unsigned(std::min<std::size_t>(length,1024*1024)));
#else
        const auto count=::write(descriptor,bytes,length);
#endif
        if(count<0&&errno==EINTR)continue;
        if(count<=0)problem("Output write failed");
        bytes+=count;length-=std::size_t(count);
    }
}
void ExclusiveOutput::finish(){
    if(descriptor<0)return;
#ifdef _WIN32
    if(_commit(descriptor)!=0)problem("Output flush failed");
    const auto result=_close(descriptor);
#else
    if(fdatasync(descriptor)!=0)problem("Output flush failed");
    const auto result=close(descriptor);
#endif
    descriptor=-1;if(result!=0)problem("Output close failed");
}
void publishNew(const fs::path &partial,const fs::path &destination){
#ifdef _WIN32
    if(!MoveFileExW(partial.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Publication failed; destination may exist and was not replaced");
#elif defined(__linux__) && defined(SYS_renameat2)
    // Linux RENAME_NOREPLACE; unlike std::filesystem::rename this cannot clobber a racing writer.
    if(syscall(SYS_renameat2,AT_FDCWD,partial.c_str(),AT_FDCWD,destination.c_str(),1)!=0)problem("Atomic no-replace publication failed");
#else
    (void)partial;(void)destination;throw std::runtime_error("No atomic no-replace publication implementation for this platform");
#endif
}
}
