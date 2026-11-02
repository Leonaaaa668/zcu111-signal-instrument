#include "tx.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
namespace {std::atomic_bool cancelled{false};void stopSignal(int){cancelled.store(true);}}
int main(int argc,char**argv){try{
 std::signal(SIGINT,stopSignal);std::signal(SIGTERM,stopSignal);
 if(argc==2&&std::string(argv[1])=="info"){std::cout<<instrument::txInfoJson(instrument::txInspect());return 0;}
 instrument::TxSession session;
 if(argc==2&&std::string(argv[1])=="dma-status"){session.diagnose();std::cout<<"TX_DMA_REGISTER_READ_PASS_NO_RESET_NO_TRANSFER\n";return 0;}
 if(argc==2&&std::string(argv[1])=="prepare"){session.prepare();std::cout<<instrument::txInfoJson(session.info());return 0;}
 if((argc==3||argc==5)&&std::string(argv[1])=="load"){
  session.upload(argv[2],cancelled);std::cout<<"TX_UPLOAD_ROUNDTRIP_SHA256_PASS_NOT_PLAYING\n"<<instrument::txInfoJson(session.info())<<std::flush;
  if(argc==5){if(std::string(argv[3])!="--play-seconds")throw std::runtime_error("Expected --play-seconds");double seconds=std::stod(argv[4]);if(!(seconds>0&&seconds<=60))throw std::runtime_error("Test playback duration must be >0 and <=60s");
   session.play();std::cout<<"TX_PLAY_STARTED\n"<<instrument::txInfoJson(session.info())<<std::flush;
   auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(int(seconds*1000));
   while(!cancelled.load()&&std::chrono::steady_clock::now()<end){std::this_thread::sleep_for(std::chrono::milliseconds(50));auto i=session.info();if(i.flags&16)throw std::runtime_error("TX hardware fault; kernel has muted output");}
   session.stop();std::cout<<"TX_STOP_CONFIRMED\n"<<instrument::txInfoJson(session.info());
  }return 0;
 }
 throw std::runtime_error("Usage: zcu111-tx info | prepare | load file.bin [--play-seconds 1]");
}catch(const std::exception&e){std::cerr<<"TX_ERROR: "<<e.what()<<'\n';return 2;}}
