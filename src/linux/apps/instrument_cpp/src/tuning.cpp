#include "tuning.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
#include <sstream>
#ifdef __linux__
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif
namespace instrument {
std::uint64_t rxExpectedWord(std::uint64_t center){
    if(center<1300000000ULL||center>2300000000ULL)throw std::runtime_error("RX center must be1300..2300MHz");
    return std::uint64_t(std::floor((2400000000.L-center)*281474976710656.L/2400000000.L));
}
bool rxWordMatches(std::uint64_t center,std::uint64_t word){
    if(center<1300000000ULL||center>2300000000ULL||word>0xffffffffffffULL)return false;
    const auto expected=rxExpectedWord(center);return word+2>=expected&&word<=expected+2;
}
RxTuning tuneReceiver(std::uint64_t center){
    rxExpectedWord(center);
#ifdef __linux__
    struct Handle{int fd=-1;~Handle(){if(fd>=0)close(fd);}} socket;
    socket.fd=::socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    if(socket.fd<0)throw std::runtime_error("Cannot create RX tuning service connection");
    timeval timeout{4,0};setsockopt(socket.fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(socket.fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    sockaddr_un address{};address.sun_family=AF_UNIX;std::strcpy(address.sun_path,"/run/zcu111-rx-tune.sock");
    if(connect(socket.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0){
        if(errno==ENOENT&&center==1800000000ULL)return {};
        throw std::runtime_error("RX tuning service unavailable; no tuning/capture performed");
    }
    const auto command=std::to_string(center)+"\n";std::size_t sent=0;
    while(sent<command.size()){const auto n=send(socket.fd,command.data()+sent,command.size()-sent,MSG_NOSIGNAL);if(n<=0)throw std::runtime_error("RX tuning request failed");sent+=std::size_t(n);}
    std::string reply;char ch;
    while(reply.size()<200){const auto n=recv(socket.fd,&ch,1,0);if(n!=1)throw std::runtime_error("RX tuning reply incomplete/timed out");if(ch=='\n')break;reply+=ch;}
    std::istringstream input(reply);std::string status,extra;RxTuning result;result.legacy=false;unsigned long long seq=0;
    if(!(input>>status>>result.center>>seq>>result.word)||status!="OK"||input>>extra||seq==0||seq>0xffffffffULL||result.center!=center||!rxWordMatches(center,result.word))throw std::runtime_error("RX tune rejected: "+reply);
    result.sequence=std::uint32_t(seq);return result;
#else
    if(center==1800000000ULL)return {};
    throw std::runtime_error("RX tuning needs the board Linux service");
#endif
}
}
