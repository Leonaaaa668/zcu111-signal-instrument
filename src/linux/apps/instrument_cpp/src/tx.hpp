#pragma once
#include "core.hpp"
#include <memory>
namespace instrument {
struct TxInfo {std::uint32_t flags=0,guard=0,copyStatus=0,playStatus=0,dacState=0;std::int32_t error=0;std::uint64_t loaded=0,received=0,maximum=0,session=0,nco=0;};
bool txAvailable();
TxInfo txInspect(); // read-only open does not stop another session
std::string txInfoJson(const TxInfo &info);
class TxSession {
 int fd=-1;
public:
 TxSession();~TxSession();TxSession(const TxSession&)=delete;TxSession& operator=(const TxSession&)=delete;
 TxInfo info()const;
 void upload(const fs::path &file,const std::atomic_bool &cancel,std::function<void(unsigned,unsigned)> progress={});
 void play();void stop();void prepare();void diagnose();
};
}
