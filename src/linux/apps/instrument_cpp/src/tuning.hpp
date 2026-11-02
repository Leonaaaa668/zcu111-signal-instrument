#pragma once
#include <cstdint>
namespace instrument {
struct RxTuning {std::uint64_t center=1800000000ULL,word=0x400000000000ULL;std::uint32_t sequence=1;bool legacy=true;};
// Positive RF center in Nyquist zone 2. The real RFDC driver handles the
// hardware programming; this function only verifies the resulting FCW.
std::uint64_t rxExpectedWord(std::uint64_t center);
bool rxWordMatches(std::uint64_t center,std::uint64_t word);
RxTuning tuneReceiver(std::uint64_t center);
}
