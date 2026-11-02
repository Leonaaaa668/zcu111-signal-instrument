#pragma once
#include "core.hpp"
namespace instrument {
struct ClockStep { Bytes data; unsigned delayMs=0; std::string kind; };
struct ClockTransport {
    std::function<std::size_t(const Bytes&)> write;
    std::function<void(unsigned)> sleep;
};
std::vector<ClockStep> clockPlan(const fs::path &clockDirectory);
std::string clockPlanJson(const std::vector<ClockStep> &steps);
void replayClockPlan(const std::vector<ClockStep> &steps,const ClockTransport &transport);
std::string listClockAdapters();
void applyClockPlan(const fs::path &adapter,const std::vector<ClockStep> &steps);
}
