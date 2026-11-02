// SPDX-License-Identifier: MIT
// Privileged ADC-only tuning service. Never initializes clocks, starts tiles,
// changes DAC state or maps /dev/mem. All RFDC changes use the official driver.
#include <xrfdc.h>
#include <metal/sys.h>
#include <metal/log.h>
#include "zcu111_sa_uapi.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <csignal>
#include <cmath>
namespace {
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
struct Fd{int value=-1;explicit Fd(int v):value(v){}~Fd(){if(value>=0)close(value);}Fd(const Fd&)=delete;};
struct Converter {
 XRFdc rf{};metal_device *device=nullptr;
 Converter(){
  XRFdc_Config config{};std::ifstream input("/sys/bus/platform/devices/b0040000.usp_rf_data_converter/of_node/param-list",std::ios::binary);
  check(bool(input),"generated RFDC config absent");input.read(reinterpret_cast<char*>(&config),sizeof(config));
  check(input.gcount()==std::streamsize(sizeof(config)),"RFDC config ABI mismatch");char extra;check(!input.get(extra),"RFDC config ABI too large");
  check(config.BaseAddr==0xb0040000ULL&&config.ADCTile_Config[1].Enable==1&&std::abs(config.ADCTile_Config[1].SamplingRate-2.4)<1e-9,"RFDC base/tile/sample-rate mismatch");
  metal_init_params params=METAL_INIT_DEFAULTS;params.log_level=METAL_LOG_WARNING;check(metal_init(&params)==0,"libmetal init failed");
  check(XRFdc_RegisterMetal(&rf,0,&device)==0,"RFDC UIO unavailable");check(XRFdc_CfgInitialize(&rf,&config)==0,"RFDC config failed");
 }
 ~Converter(){if(device)metal_device_close(device);metal_finish();}
 zsa_tune_state tune(unsigned long long center){
  check(center>=1300000000ULL&&center<=2300000000ULL,"center outside1300..2300MHz");
  Fd fd(open("/dev/zcu111_spectrum",O_RDONLY|O_CLOEXEC));check(fd.value>=0,"RX busy or unavailable; stop acquisition before tuning");
  zsa_tune_state state{};check(ioctl(fd.value,ZSA_IOC_TUNE_STATE,&state)==0,"RX driver ABI2 required");
  if(state.flags==ZSA_TUNE_VALID&&state.center_hz==center)return state;
  // Verify existing readiness without restarting a tile or changing clocks.
  XRFdc_IPStatus status{};XRFdc_PLL_Settings pll{};u32 decimation=0;XRFdc_Mixer_Settings mixer{};
  check(XRFdc_GetIPStatus(&rf,&status)==0&&status.ADCTileStatus[1].TileState==15,"ADC not ready");
  check(XRFdc_GetPLLConfig(&rf,XRFDC_ADC_TILE,1,&pll)==0&&std::abs(pll.SampleRate-2.4)<1e-6,"ADC sample rate changed");
  check(XRFdc_GetDecimationFactor(&rf,1,1,&decimation)==0&&decimation==2,"ADC decimation changed");
  check(XRFdc_GetMixerSettings(&rf,XRFDC_ADC_TILE,1,1,&mixer)==0&&mixer.MixerType==XRFDC_MIXER_TYPE_FINE&&mixer.MixerMode==XRFDC_MIXER_MODE_R2C,"ADC mixer mode changed");
  check(ioctl(fd.value,ZSA_IOC_TUNE_BEGIN)==0,"cannot quiesce RX");
  // Failure after BEGIN deliberately leaves capture blocked. Only a later
  // successful complete tuning transaction can make the device valid again.
  mixer.Freq=double(center)/1e6;mixer.EventSource=XRFDC_EVNT_SRC_TILE;
  check(XRFdc_SetMixerSettings(&rf,XRFDC_ADC_TILE,1,1,&mixer)==0,"RFDC SetMixerSettings failed; RX remains blocked");
  check(XRFdc_UpdateEvent(&rf,XRFDC_ADC_TILE,1,1,XRFDC_EVENT_MIXER)==0,"RFDC tile mixer event failed; RX remains blocked");
  state={};state.center_hz=center;
  check(ioctl(fd.value,ZSA_IOC_TUNE_COMMIT,&state)==0,"RFDC readback/commit failed; RX remains blocked");
  return state;
 }
};
}
int main(int argc,char **argv){try{
 check(geteuid()==0,"run the tuning service as root");
 check(argc==1,"usage: zcu111-rx-tuned (foreground; no RF output)");(void)argv;
 std::signal(SIGPIPE,SIG_IGN);
 Fd lock(open("/run/zcu111-rx-tune.lock",O_CREAT|O_RDWR|O_CLOEXEC,0600));check(lock.value>=0&&flock(lock.value,LOCK_EX|LOCK_NB)==0,"tuning service already running");
 Converter converter;
 {Fd probe(open("/dev/zcu111_spectrum",O_RDONLY|O_CLOEXEC));zsa_tune_state state{};check(probe.value>=0&&ioctl(probe.value,ZSA_IOC_TUNE_STATE,&state)==0,"RX ABI2 required before starting tuning service");}
 const char *path="/run/zcu111-rx-tune.sock";
 struct stat existing{};if(lstat(path,&existing)==0){check(S_ISSOCK(existing.st_mode)&&existing.st_uid==0,"unexpected existing socket path");check(unlink(path)==0,"cannot remove stale tuning socket");}
 Fd server(socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0));check(server.value>=0,"socket failed");
 sockaddr_un address{};address.sun_family=AF_UNIX;std::strcpy(address.sun_path,path);
 const auto *group=getgrnam("video");check(group,"video group absent");
 mode_t old=umask(0077);const int bound=bind(server.value,reinterpret_cast<sockaddr*>(&address),sizeof(address));umask(old);check(bound==0,"bind failed");
 check(chown(path,0,group->gr_gid)==0&&chmod(path,0660)==0&&listen(server.value,4)==0,"socket permissions/listen failed");
 std::cout<<"RX_TUNE_SERVICE_READY 1300..2300MHz; no RF playback"<<std::endl;
 for(;;){
  Fd client(accept4(server.value,nullptr,nullptr,SOCK_CLOEXEC));if(client.value<0)continue;
  timeval timeout{3,0};setsockopt(client.value,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(client.value,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
  std::string response;
  try{
   std::string request;char ch=0;while(request.size()<32){check(recv(client.value,&ch,1,0)==1,"request timeout/incomplete");if(ch=='\n')break;check(ch>='0'&&ch<='9',"request must be integerHz");request+=ch;}
   check(ch=='\n'&&!request.empty(),"invalid request length");const auto state=converter.tune(std::stoull(request));
   response="OK "+std::to_string(state.center_hz)+" "+std::to_string(state.nco_update_seq)+" "+std::to_string(state.nco_word)+"\n";
  }catch(const std::exception &e){response="ERROR "+std::string(e.what())+"\n";std::cerr<<response;}
  std::size_t sent=0;while(sent<response.size()){const auto n=send(client.value,response.data()+sent,response.size()-sent,MSG_NOSIGNAL);if(n<=0)break;sent+=std::size_t(n);}
 }
}catch(const std::exception &e){std::cerr<<"RX_TUNE_SERVICE_ERROR: "<<e.what()<<'\n';return 2;}}
