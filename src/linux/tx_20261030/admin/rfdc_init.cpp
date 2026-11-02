// SPDX-License-Identifier: MIT
// Official Linux xrfdc/libmetal API, accessed through a dedicated UIO mapping.
#include <xrfdc.h>
#include <metal/log.h>
#include <metal/sys.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
namespace {
void check(bool ok,const char *what){if(!ok)throw std::runtime_error(what);}
u64 word(XRFdc &rf){XRFdc *p=&rf;const auto base=XRFDC_BLOCK_BASE(XRFDC_DAC_TILE,1,0);return (u64(XRFdc_ReadReg16(p,base,XRFDC_ADC_NCO_FQWD_UPP_OFFSET))<<32)|(u64(XRFdc_ReadReg16(p,base,XRFDC_ADC_NCO_FQWD_MID_OFFSET))<<16)|XRFdc_ReadReg16(p,base,XRFDC_ADC_NCO_FQWD_LOW_OFFSET);}
}
int main(int argc,char **argv){
 try{
  check(geteuid()==0,"RFDC initialization requires root");
  check(argc==2&&std::string(argv[1])=="--initialize-stopped","Usage: zcu111-rfdc-init --initialize-stopped (TX gate must be stopped)");
  XRFdc_Config cfg{};std::ifstream in("/sys/bus/platform/devices/b0040000.usp_rf_data_converter/of_node/param-list",std::ios::binary);
  check(bool(in),"RFDC generated param-list absent");in.read(reinterpret_cast<char*>(&cfg),sizeof(cfg));check(in.gcount()==sizeof(cfg),"RFDC configuration ABI size mismatch");char extra;check(!in.get(extra),"RFDC config is larger than current vendor ABI");
  check(cfg.BaseAddr==0xb0040000ULL,"Wrong RFDC base address");
  check(cfg.DACTile_Config[1].Enable==1&&std::abs(cfg.DACTile_Config[1].SamplingRate-2.4)<1e-9,"Wrong DAC tile/sample-rate contract");
  struct metal_init_params params=METAL_INIT_DEFAULTS;params.log_level=METAL_LOG_WARNING;check(metal_init(&params)==0,"libmetal initialization failed");
  XRFdc rf{};struct metal_device *dev=nullptr;
  check(XRFdc_RegisterMetal(&rf,0,&dev)==XRFDC_SUCCESS,"RFDC UIO registration failed");
  check(XRFdc_CfgInitialize(&rf,&cfg)==XRFDC_SUCCESS,"RFDC configuration initialization failed");
  check(XRFdc_StartUp(&rf,XRFDC_DAC_TILE,1)==XRFDC_SUCCESS,"DAC tile startup failed");
  check(XRFdc_SetupFIFO(&rf,XRFDC_DAC_TILE,1,1)==XRFDC_SUCCESS,"DAC FIFO initialization failed");
  u32 interpolation=0,source=0,words=0,zone=0;XRFdc_PLL_Settings pll{};XRFdc_Mixer_Settings mixer{};XRFdc_IPStatus status{};
  check(XRFdc_GetInterpolationFactor(&rf,1,0,&interpolation)==0,"Read DAC interpolation failed");
  check(XRFdc_GetClockSource(&rf,XRFDC_DAC_TILE,1,&source)==0,"Read DAC clock source failed");
  check(XRFdc_GetFabWrVldWords(&rf,XRFDC_DAC_TILE,1,0,&words)==0,"Read DAC words failed");
  check(XRFdc_GetNyquistZone(&rf,XRFDC_DAC_TILE,1,0,&zone)==0,"Read DAC Nyquist zone failed");
  check(XRFdc_GetPLLConfig(&rf,XRFDC_DAC_TILE,1,&pll)==0,"Read DAC PLL config failed");
  check(XRFdc_GetMixerSettings(&rf,XRFDC_DAC_TILE,1,0,&mixer)==0,"Read DAC mixer failed");
  check(XRFdc_GetIPStatus(&rf,&status)==0,"Read DAC tile state failed");
  std::cout<<"DAC_STATE="<<status.DACTileStatus[1].TileState<<" INTERPOLATION="<<interpolation<<" WORDS="<<words<<" ZONE="<<zone<<" FS_GSPS="<<pll.SampleRate<<" NCO_WORD="<<std::hex<<word(rf)<<std::dec<<'\n'<<std::flush;
  check(interpolation==2&&source==XRFDC_EXTERNAL_CLK&&words==8&&zone==2&&status.DACTileStatus[1].TileState==15,"DAC hardware geometry/readiness mismatch");
  check(std::abs(pll.SampleRate-2.4)<1e-6&&mixer.MixerType==XRFDC_MIXER_TYPE_FINE&&mixer.MixerMode==XRFDC_MIXER_MODE_C2R,"DAC sample-rate or mixer mismatch");
  auto initial=word(rf);check(initial==0x400000000000ULL,"DAC NCO does not match frozen 1.8GHz/2.4GSps/zone2 contract");
  mixer.Freq=1800.;mixer.EventSource=XRFDC_EVNT_SRC_IMMEDIATE;
  check(XRFdc_SetMixerSettings(&rf,XRFDC_DAC_TILE,1,0,&mixer)==0,"Canonical DAC mixer request failed");
  check(XRFdc_GetMixerSettings(&rf,XRFDC_DAC_TILE,1,0,&mixer)==0&&std::abs(mixer.Freq-1800.)<.001&&word(rf)==initial,"DAC tuning changed unexpectedly");
  metal_device_close(dev);metal_finish();std::cout<<"DAC_INIT_CONTRACT_PASS_NOT_PLAYING\n";return 0;
 }catch(const std::exception &e){std::cerr<<"RFDC_INIT_ERROR: "<<e.what()<<'\n';return 2;}
}
