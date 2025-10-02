#include "cpu_features.h"

#include <cstdio>
#include <cstdlib>

#ifdef _MSC_VER
  #include <intrin.h>
#else
  #include <cpuid.h>
#endif

namespace {

void cpuid(int a, int c, int info[4]) {
#ifdef _MSC_VER
    __cpuidex(info, a, c);
#else
    __cpuid_count(a, c, info[0], info[1], info[2], info[3]);
#endif
}

} // namespace

CpuFeatures detectCpuFeatures() {
    CpuFeatures f{};
    int i[4]{};
    cpuid(1, 0, i);
    int ecx = i[2];
    int edx = i[3];
    f.sse2  = (edx & (1 << 26)) != 0;
    f.ssse3 = (ecx & (1 << 9))  != 0;
    f.sse41 = (ecx & (1 << 19)) != 0;
    f.sse42 = (ecx & (1 << 20)) != 0;
    f.popcnt= (ecx & (1 << 23)) != 0;
    f.avx   = (ecx & (1 << 28)) != 0;

    cpuid(7, 0, i);
    int ebx = i[1];
    f.avx2  = (ebx & (1 << 5))  != 0;
    f.bmi2  = (ebx & (1 << 8))  != 0;
    return f;
}

std::string CpuFeatures::toString() const {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "sse2=%d ssse3=%d sse41=%d sse42=%d popcnt=%d avx=%d avx2=%d bmi2=%d",
        sse2, ssse3, sse41, sse42, popcnt, avx, avx2, bmi2);
    return buf;
}

void requireSupportedOrExit(const CpuFeatures& f) {
#if defined(SIRIOC_REQUIRE_SSE41)
    if (!(f.sse41 && f.popcnt)) {
        std::fprintf(stderr, "info string FATAL: This build requires SSE4.1+POPCNT. Detected: %s\n",
            f.toString().c_str());
        std::fflush(stderr);
        std::exit(1);
    }
#endif
#if defined(SIRIOC_REQUIRE_AVX2)
    if (!(f.avx2 && f.bmi2)) {
        std::fprintf(stderr, "info string FATAL: This build requires AVX2+BMI2. Detected: %s\n",
            f.toString().c_str());
        std::fflush(stderr);
        std::exit(1);
    }
#endif
}
