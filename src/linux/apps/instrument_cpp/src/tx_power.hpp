#pragma once
#include <string>
namespace instrument {
struct TxCwCalibration {
 std::string profileSha,verificationSha,setupId,referencePlane;
 double frequency=1.79e9,slope=0,dbmAt2048=0,minRms=0,maxRms=0;
 double commandOffsetDb=0,minimumDbm=-50,maximumDbm=-10;
 std::string contextSha,contextVerificationSha;
};
double txCwPowerDbm(const TxCwCalibration &model,double rms);
double txCwRms(const TxCwCalibration &model,double frequency,double requestedDbm);
double dbmFromDbw(double dbw);
double dbwFromDbm(double dbm);
}
