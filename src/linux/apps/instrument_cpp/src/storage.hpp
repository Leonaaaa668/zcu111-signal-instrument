#pragma once
#include "core.hpp"
namespace instrument {
class ExclusiveOutput {
    int descriptor=-1;
public:
    explicit ExclusiveOutput(const fs::path &path);
    ~ExclusiveOutput();
    ExclusiveOutput(const ExclusiveOutput&)=delete;
    ExclusiveOutput& operator=(const ExclusiveOutput&)=delete;
    void write(const void *data,std::size_t length);
    void finish();
};
void publishNew(const fs::path &partial,const fs::path &destination);
}
