#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace mixxx::androidusb {

constexpr std::size_t kChannels = 4;
constexpr std::size_t kRingFrames = 32768;

inline int16_t floatToPcm16(float sample) {
    if ((std::bit_cast<uint32_t>(sample) & 0x7f800000u) == 0x7f800000u) {
        return 0;
    }
    const float clamped = std::clamp(sample, -1.0f, 1.0f);
    return clamped < 0.0f ? static_cast<int16_t>(clamped * 32768.0f)
                          : static_cast<int16_t>(clamped * 32767.0f);
}

class PcmRing {
  public:
    void reset() {
        m_read.store(0, std::memory_order_relaxed);
        m_written.store(0, std::memory_order_relaxed);
    }

    std::size_t availableFrames() const {
        const auto written = m_written.load(std::memory_order_acquire);
        const auto read = m_read.load(std::memory_order_acquire);
        return static_cast<std::size_t>(written - read);
    }

    std::size_t freeFrames() const {
        return kRingFrames - availableFrames();
    }

    bool pushFloat(const float* samples, std::size_t frames) {
        const auto written = m_written.load(std::memory_order_relaxed);
        const auto read = m_read.load(std::memory_order_acquire);
        if (frames > kRingFrames - static_cast<std::size_t>(written - read)) {
            return false;
        }
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const auto base = ((written + frame) % kRingFrames) * kChannels;
            for (std::size_t channel = 0; channel < kChannels; ++channel) {
                m_samples[base + channel] = floatToPcm16(samples[frame * kChannels + channel]);
            }
        }
        m_written.store(written + frames, std::memory_order_release);
        return true;
    }

    std::size_t popBytes(uint8_t* output, std::size_t frames) {
        const auto read = m_read.load(std::memory_order_relaxed);
        const auto written = m_written.load(std::memory_order_acquire);
        const auto available = static_cast<std::size_t>(written - read);
        const auto copied = std::min(frames, available);
        for (std::size_t frame = 0; frame < copied; ++frame) {
            const auto base = ((read + frame) % kRingFrames) * kChannels;
            for (std::size_t channel = 0; channel < kChannels; ++channel) {
                const auto value = static_cast<uint16_t>(m_samples[base + channel]);
                const auto offset = (frame * kChannels + channel) * sizeof(int16_t);
                output[offset] = static_cast<uint8_t>(value);
                output[offset + 1] = static_cast<uint8_t>(value >> 8);
            }
        }
        std::fill(output + copied * kChannels * sizeof(int16_t),
                output + frames * kChannels * sizeof(int16_t),
                0);
        m_read.store(read + copied, std::memory_order_release);
        return copied;
    }

  private:
    std::array<int16_t, kRingFrames * kChannels> m_samples{};
    std::atomic<uint64_t> m_written{0};
    std::atomic<uint64_t> m_read{0};
};

}
