#pragma once
#include <cstdint>
namespace instrument {
// UI-thread-only generation token: invalidates callbacks from an earlier repeat request.
class RepeatGate {
    std::uint64_t generation=0;
    bool repeating=false;
public:
    std::uint64_t begin(bool repeat){repeating=repeat;return ++generation;}
    void stop(){repeating=false;++generation;}
    bool enabled()const{return repeating;}
    bool permits(std::uint64_t token)const{return repeating&&token==generation;}
};
}
