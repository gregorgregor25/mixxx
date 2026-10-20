#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace mixxx::flx6filter {

constexpr double kMildQ = 0.5;
constexpr double kFlatQ = 0.707106781;
constexpr double kWildQ = 5.623413251903491;
constexpr double kFlatPosition = 0.7;
constexpr int kResonanceScaleVersion = 4;

inline bool finiteBits(double value) {
    return (std::bit_cast<std::uint64_t>(value) & UINT64_C(0x7ff0000000000000)) !=
            UINT64_C(0x7ff0000000000000);
}

inline double clampNormalized(double value) {
    return finiteBits(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

inline double qFromNormalized(double normalized) {
    const double position = clampNormalized(normalized);
    if (position == 0.0) {
        return kMildQ;
    }
    if (position == kFlatPosition) {
        return kFlatQ;
    }
    if (position == 1.0) {
        return kWildQ;
    }
    if (position <= kFlatPosition) {
        return kMildQ * std::pow(kFlatQ / kMildQ, position / kFlatPosition);
    }
    return kFlatQ * std::pow(kWildQ / kFlatQ,
            (position - kFlatPosition) / (1.0 - kFlatPosition));
}

inline double normalizedFromQ(double q) {
    if (!finiteBits(q)) {
        return 0.0;
    }
    const double boundedQ = std::clamp(q, kMildQ, kWildQ);
    if (boundedQ == kMildQ) {
        return 0.0;
    }
    if (boundedQ == kWildQ) {
        return 1.0;
    }
    if (boundedQ <= kFlatQ) {
        return kFlatPosition * std::log(boundedQ / kMildQ) /
                std::log(kFlatQ / kMildQ);
    }
    return kFlatPosition + (1.0 - kFlatPosition) *
            std::log(boundedQ / kFlatQ) / std::log(kWildQ / kFlatQ);
}

inline double legacyQFromNormalized(int version, double normalized) {
    const double oldWildQ = version < 2 ? 2.0 : version < 3 ? 4.0 : 8.0;
    return kFlatQ + (oldWildQ - kFlatQ) * clampNormalized(normalized);
}

}
