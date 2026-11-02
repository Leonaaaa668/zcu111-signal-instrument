#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
namespace instrument {
// One pending frame only: slow painting never queues an unbounded history.
template<class T> class LatestFrame {
    std::mutex mutex;
    std::shared_ptr<const T> pending;
    std::uint64_t replaced_=0;
public:
    void publish(T value){auto next=std::make_shared<const T>(std::move(value));std::lock_guard<std::mutex> lock(mutex);if(pending)++replaced_;pending=std::move(next);}
    std::shared_ptr<const T> take(){std::lock_guard<std::mutex> lock(mutex);auto out=std::move(pending);return out;}
    void clear(){std::lock_guard<std::mutex> lock(mutex);pending.reset();replaced_=0;}
    std::uint64_t replaced(){std::lock_guard<std::mutex> lock(mutex);return replaced_;}
};
}
