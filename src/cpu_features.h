#pragma once

#include <string>

struct CpuFeatures {
    bool sse2{};
    bool ssse3{};
    bool sse41{};
    bool sse42{};
    bool popcnt{};
    bool avx{};
    bool avx2{};
    bool bmi2{};
    std::string toString() const;
};

CpuFeatures detectCpuFeatures();
void requireSupportedOrExit(const CpuFeatures& f);
