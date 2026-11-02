// SPDX-License-Identifier: MIT
// ADC-only initialization using matching official librfdc. Does not start TX.
#include <xrfdc.h>
#include <metal/log.h>
#include <metal/sys.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "zcu111_tx_uapi.h"
namespace {
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
u64 word(XRFdc &rf,unsigned slice){XRFdc *p=&rf;auto base=XRFDC_BLOCK_BASE(XRFDC_ADC_TILE,1,slice);return (u64(XRFdc_ReadReg16(p,base,0x94))<<32)|(u64(XRFdc_ReadReg16(p,base,0x98))<<16)|XRFdc_ReadReg16(p,base,0x9c);}
}
int main(int argc,char **argv){try{
 check(geteuid()==0&&argc==2&&std::string(argv[1])=="--initialize-adc-stopped","Usage: root zcu111-rfdc-rx-init --initialize-adc-stopped");
 int tx=open("/dev/zcu111_tx",O_RDONLY|O_CLOEXEC);check(tx>=0,"TX initialization must hold the control clock first");
 ztx_info txinfo{};check(ioctl(tx,ZTX_IOC_INFO,&txinfo)==0&&txinfo.abi_version==1&&(txinfo.flags&ZTX_F_PREPARED)&&!(txinfo.flags&ZTX_F_PLAYING),"Stop TX before ADC initialization");
 XRFdc_Config cfg{};std::ifstream in("/sys/bus/platform/devices/b0040000.usp_rf_data_converter/of_node/param-list",std::ios::binary);
 check(bool(in),"Generated RFDC configuration absent");in.read(reinterpret_cast<char*>(&cfg),sizeof(cfg));check(in.gcount()==std::streamsize(sizeof(cfg)),"RFDC configuration ABI differs");char extra;check(!in.get(extra),"RFDC configuration too large");
 check(cfg.BaseAddr==0xb0040000ULL&&cfg.ADCTile_Config[1].Enable==1&&std::abs(cfg.ADCTile_Config[1].SamplingRate-2.4)<1e-9,"Wrong ADC tile/base/sample-rate contract");
 metal_init_params params=METAL_INIT_DEFAULTS;params.log_level=METAL_LOG_WARNING;check(metal_init(&params)==0,"libmetal initialization failed");
 XRFdc rf{};metal_device *dev=nullptr;check(XRFdc_RegisterMetal(&rf,0,&dev)==0,"RFDC UIO registration failed");check(XRFdc_CfgInitialize(&rf,&cfg)==0,"RFDC configuration failed");
 check(XRFdc_StartUp(&rf,XRFDC_ADC_TILE,1)==0,"ADC tile startup failed");check(XRFdc_SetupFIFO(&rf,XRFDC_ADC_TILE,1,1)==0,"ADC FIFO setup failed");
 // Dual-ADC public API uses physical block 1; DRP NCO slices remain 2/3.
 u32 decimation=0,source=0,words=0,zone=0;XRFdc_PLL_Settings pll{};XRFdc_Mixer_Settings mixer{};XRFdc_IPStatus status{};
 check(XRFdc_GetDecimationFactor(&rf,1,1,&decimation)==0,"Read ADC decimation failed");
 check(XRFdc_GetClockSource(&rf,XRFDC_ADC_TILE,1,&source)==0,"Read ADC clock failed");
 check(XRFdc_GetFabRdVldWords(&rf,XRFDC_ADC_TILE,1,1,&words)==0,"Read ADC AXIS words failed");
 check(XRFdc_GetNyquistZone(&rf,XRFDC_ADC_TILE,1,1,&zone)==0,"Read ADC Nyquist zone failed");
 check(XRFdc_GetPLLConfig(&rf,XRFDC_ADC_TILE,1,&pll)==0,"Read ADC sample rate failed");
 check(XRFdc_GetMixerSettings(&rf,XRFDC_ADC_TILE,1,1,&mixer)==0,"Read ADC mixer failed");
 check(XRFdc_GetIPStatus(&rf,&status)==0,"Read ADC tile status failed");
 std::cout<<"ADC_STATE="<<status.ADCTileStatus[1].TileState<<" DECIMATION="<<decimation<<" WORDS="<<words<<" ZONE="<<zone<<" FS_GSPS="<<pll.SampleRate<<" NCO2="<<std::hex<<word(rf,2)<<" NCO3="<<word(rf,3)<<std::dec<<'\n'<<std::flush;
 check(status.ADCTileStatus[1].TileState==15&&decimation==2&&source==XRFDC_EXTERNAL_CLK&&words==4&&zone==2&&std::abs(pll.SampleRate-2.4)<1e-6,"ADC hardware geometry/readiness mismatch");
 check(mixer.MixerType==XRFDC_MIXER_TYPE_FINE&&mixer.MixerMode==XRFDC_MIXER_MODE_R2C&&word(rf,2)==0x400000000000ULL&&word(rf,3)==0x400000000000ULL,"ADC NCO/mixer contract mismatch");
 metal_device_close(dev);metal_finish();close(tx);std::cout<<"ADC_INIT_CONTRACT_PASS_NO_CAPTURE_NO_PLAY\n";return 0;
}catch(const std::exception&e){std::cerr<<"ADC_INIT_ERROR: "<<e.what()<<'\n';return 2;}}
