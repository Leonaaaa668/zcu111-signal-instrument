#pragma once
#include <array>
#include <vector>
namespace instrument {
struct VideoPlan {unsigned frames=1;double requestedHz=0,equivalentHz=0,rbwEnbw=0;};
// FFT video-average equivalence, not a certified continuous-time3dB lowpass.
VideoPlan planVideoAverages(double rbwEnbw,unsigned minimumFrames,double requestedHz);
double equivalentVideoBandwidth(double rbwEnbw,unsigned frames);
struct MarkerPoint {bool enabled=false,tracking=false,valid=false,positioned=false;double frequency=0,level=-300;};
struct MarkerDelta {bool valid=false;double frequency=0,level=0;};
class MarkerBank {
 std::vector<double> frequencies,levels;double low=0,high=0;
 unsigned nearest(double frequency)const;
 void refresh(unsigned id);
public:
 std::array<MarkerPoint,4> points{};unsigned active=0,reference=0;bool deltaEnabled=false;
 MarkerBank(){points[0].enabled=true;points[0].tracking=true;}
 void setTrace(const std::vector<double> &rf,const std::vector<double> &db,double viewLow,double viewHigh);
 void setView(double viewLow,double viewHigh);
 void enable(unsigned id,bool on);
 void position(unsigned id,double frequency);
 void track(unsigned id,bool on);
 void peak(unsigned id);
 unsigned fourPeaks(double separationHz);
 MarkerDelta delta(unsigned id)const;
};
}
