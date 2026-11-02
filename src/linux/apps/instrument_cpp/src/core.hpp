#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include "tx_power.hpp"

namespace instrument {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
struct Receipt {
    std::uint32_t ticket=0, epoch=0, segment=0, ncoSeq=0, logn=0, zoom=0;
    std::uint64_t sequence=0, counterEpoch=0;
};
struct WindowMetrics { std::uint64_t sum=0, squareSum=0; };
struct Resources {
    std::map<unsigned, WindowMetrics> windows;
    std::array<std::uint8_t,32> fingerprint{};
};
struct Packet {
    Bytes bytes;
    std::array<std::uint32_t,64> header{};
    std::vector<std::uint64_t> power;
    Receipt receipt;
    unsigned n=0;
    std::string hash;
};
struct Measurement {
    unsigned n=0, frames=0, peak=0;
    std::uint32_t ncoSequence=0;
    std::uint64_t ncoWord=0;
    double fs=150e6, center=1.8e9, spacing=0, enbwBins=0, enbwHz=0;
    double peakHz=0, toneHz=0, peakDbfs=-300, mainlobeDbfs=-300, totalDbfs=-300;
    bool toneValid=false;
    std::vector<double> average, coherent, energy, displayDbfs;
    std::vector<std::string> hashes;
};
struct CaptureRequest {
    bool live=true, archive=true;
    unsigned n=8192, frames=1;
    std::uint64_t centerHz=1800000000ULL;
    fs::path output;
    std::vector<fs::path> replay;
};
struct CaptureResult {
    Measurement measurement;
    fs::path output;
    bool live=false, cancelled=false, hasData=false;
    double readMs=0, validateMs=0, measureMs=0, archiveMs=0;
    std::uint64_t firstSequence=0, lastSequence=0, lastCounterEpoch=0;
};
Bytes readFile(const fs::path &path, std::size_t limit=1024*1024);
void writeFile(const fs::path &path, const Bytes &data);
std::string sha256(const Bytes &data);
Resources loadResources(const fs::path &root);
Receipt loadReceipt(const fs::path &path);
std::string receiptJson(const Receipt &receipt);
Packet decode(Bytes bytes, const Receipt &receipt, const Resources &resources);
Measurement measure(const std::vector<Packet> &packets);
// Merge independent acquisitions in linear power, retaining tune identity.
void mergeMeasurements(Measurement &accumulator,const Measurement &next);
std::string measurementJson(const Measurement &measurement);
void saveMeasurement(const fs::path &directory, const Measurement &measurement);
bool rxAvailable();
CaptureResult capture(const CaptureRequest &request, const Resources &resources,
                      std::atomic_bool &cancel, std::function<void(unsigned,unsigned)> progress={});

// Bounded-rate spectrum frames. The existing FPGA snapshot engine has dead time;
// this is continuous display acquisition, not gapless ADC/IQ transport.
void streamSpectra(CaptureRequest request, const Resources &resources,
                   std::atomic_bool &cancel,
                   const std::function<void(CaptureResult &&,std::uint64_t)> &publish,
                   unsigned periodMs=50);

struct WaveRequest {
    std::string type="CW";
    double sampleRate=1.2e9, duration=.010, offset=10e6;
    double pulseWidth=10e-6, period=100e-6, bandwidth=20e6, symbolRate=5e6, rolloff=.35;
    unsigned order=16, peak=2048;
    std::uint32_t seed=1701;
    fs::path output;
    std::optional<TxCwCalibration> powerModel;
    double targetDbm=-40;
};
void generateWaveform(const WaveRequest &request, std::atomic_bool &cancel,
                      std::function<void(unsigned,unsigned)> progress={});
}
