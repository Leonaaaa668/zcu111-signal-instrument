#include "rx_power_qt.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
// Real ADC packet audit. Deliberately separate from synthetic/unit tests.
int main(int argc,char **argv){try{
    if(argc!=4)throw std::runtime_error("usage: resources profile measured_evidence_directory");
    const auto resources=instrument::loadResources(argv[1]);
    const auto profile=loadRxCwCalibration(QString::fromLocal8Bit(argv[2]),resources);
    const instrument::fs::path base=argv[3];
    const auto readBatch=[&](const instrument::fs::path &folder,std::vector<instrument::Packet> &packets){
        for(unsigned i=0;i<16;++i){std::ostringstream name;name<<"frame_"<<std::setw(4)<<std::setfill('0')<<i<<".bin";const auto p=folder/name.str();
            packets.push_back(instrument::decode(instrument::readFile(p),instrument::loadReceipt(p.string()+".receipt.json"),resources));}
    };
    for(const auto &id:{"p01","p03","p05","p07"}){
        std::vector<instrument::Packet> packets;
        for(char batch='a';batch<='c';++batch)readBatch(base/(std::string("signal_")+id+"_"+batch),packets);
        const auto m=instrument::measure(packets);const auto reading=instrument::rxCwPower(m,profile);
        QFile file(QString::fromStdString((base/(std::string(id)+"_reference_user_report.json")).string()));
        if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("missing reference report");
        const auto reference=QJsonDocument::fromJson(file.readAll()).object().value("reference_linear_mean_dbm");
        if(!reference.isDouble()||!reading.valid||std::abs(reading.dbm-reference.toDouble())>.1)throw std::runtime_error(std::string(id)+" real ADC calibration check failed: "+reading.reason);
        for(char batch='a';batch<='c';++batch){std::vector<instrument::Packet> individual;readBatch(base/(std::string("signal_")+id+"_"+batch),individual);
            const auto check=instrument::rxCwPower(instrument::measure(individual),profile);
            if(!check.valid||std::abs(check.dbm-reference.toDouble())>.1)throw std::runtime_error(std::string(id)+" 16-frame batch exceeds screening limit");}
        std::cout<<id<<" frames="<<m.frames<<" input_dbm="<<std::setprecision(12)<<reading.dbm<<" reference_dbm="<<reference.toDouble()<<" error_db="<<reading.dbm-reference.toDouble()<<'\n';
    }
    std::vector<instrument::Packet> noise;readBatch(base/"baseline_off",noise);
    if(instrument::rxCwPower(instrument::measure(noise),profile).valid)throw std::runtime_error("RF OFF baseline incorrectly given calibrated CW power");
    std::cout<<"RX_MEASURED_PACKET_REPLAY_PASS; RF OFF rejected; holdout excluded from fit\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 2;}}
