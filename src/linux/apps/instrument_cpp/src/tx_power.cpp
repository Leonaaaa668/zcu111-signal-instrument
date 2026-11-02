#include "tx_power.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>
namespace instrument {
namespace {void valid(const TxCwCalibration &m){
 auto hex=[](const std::string &s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
 auto safe=[](const std::string &s){return !s.empty()&&s.size()<=160&&std::all_of(s.begin(),s.end(),[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';});};
 if(!hex(m.profileSha)||!hex(m.verificationSha)||!safe(m.setupId)||!safe(m.referencePlane))throw std::runtime_error("Invalid TX calibration identity");
 if(!std::isfinite(m.commandOffsetDb)||std::abs(m.commandOffsetDb)>1||!std::isfinite(m.minimumDbm)||!std::isfinite(m.maximumDbm)||m.minimumDbm<-50||m.maximumDbm>-10||m.minimumDbm>=m.maximumDbm)throw std::runtime_error("Invalid CW context range/offset");
 if(m.contextSha.empty()){if(m.commandOffsetDb!=0||m.minimumDbm!=-50||m.maximumDbm!=-10||!m.contextVerificationSha.empty())throw std::runtime_error("CW correction has no bound verification");}
 else if(!hex(m.contextSha)||!hex(m.contextVerificationSha))throw std::runtime_error("Invalid CW context verification identity");
 if(m.frequency!=1.79e9||!std::isfinite(m.slope)||m.slope<.98||m.slope>1.02||!std::isfinite(m.dbmAt2048)||m.dbmAt2048<-100||m.dbmAt2048>0||!std::isfinite(m.minRms)||!std::isfinite(m.maxRms)||m.minRms<=0||m.minRms>=m.maxRms||m.maxRms>32701||m.profileSha.size()!=64||m.verificationSha.size()!=64||m.setupId.empty()||m.referencePlane.empty())throw std::runtime_error("Invalid TX CW calibration model");
}}
double txCwPowerDbm(const TxCwCalibration &m,double rms){valid(m);if(!std::isfinite(rms)||rms<m.minRms||rms>m.maxRms)throw std::runtime_error("TX RMS outside measured model range");return m.dbmAt2048+20*m.slope*std::log10(rms/2048)-m.commandOffsetDb;}
double txCwRms(const TxCwCalibration &m,double frequency,double requested){valid(m);
 if(frequency!=m.frequency||!std::isfinite(requested)||requested<m.minimumDbm||requested>m.maximumDbm)throw std::runtime_error("CW target outside selected1790MHz reference/range");
 const double baseTarget=requested+m.commandOffsetDb;
 if(baseTarget<-50||baseTarget>-10)throw std::runtime_error("Context command outside original measured range");
 const double rms=2048*std::pow(10.,(baseTarget-m.dbmAt2048)/(20*m.slope));txCwPowerDbm(m,rms);return rms;
}
double dbmFromDbw(double value){if(!std::isfinite(value))throw std::runtime_error("Invalid dBW");return value+30;}
double dbwFromDbm(double value){if(!std::isfinite(value))throw std::runtime_error("Invalid dBm");return value-30;}
}
