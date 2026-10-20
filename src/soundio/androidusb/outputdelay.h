#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mixxx::androidusb {

class OutputDelay final {
  public:
    static constexpr uint32_t kSampleRate = 44100;
    static constexpr uint32_t kMaximumMs = 1000;
    static constexpr uint32_t kMaximumFrames = kSampleRate;
    static constexpr uint32_t kHistoryFrames = kMaximumFrames + 1;
    static constexpr uint32_t kTransitionFrames = 441;

    static uint32_t normalizedMs(double value) {
        if ((std::bit_cast<uint64_t>(value) & 0x7ff0000000000000ULL) ==
                0x7ff0000000000000ULL) {
            return 0;
        }
        return static_cast<uint32_t>(std::llround(
                std::clamp(value, 0.0, static_cast<double>(kMaximumMs))));
    }

    static uint32_t framesForMs(double value) {
        const uint32_t milliseconds = normalizedMs(value);
        return (milliseconds * kSampleRate + 500) / 1000;
    }

    void prepare(double leftMs, double rightMs, double headphonesMs, bool bypass) {
        m_history.assign(static_cast<std::size_t>(kHistoryFrames) * 4, 0.0f);
        m_writeFrame = 0;
        m_lanes = {};
        m_requested = {
                framesForMs(leftMs), framesForMs(rightMs), framesForMs(headphonesMs)};
        for (std::size_t lane = 0; lane < m_lanes.size(); ++lane) {
            m_lanes[lane].activeFrames = m_requested[lane];
        }
        m_requestedWet = !bypass;
        m_targetWet = m_requestedWet;
        m_wetMix = m_requestedWet ? 1.0f : 0.0f;
        m_bypassFromMix = m_wetMix;
        m_bypassRemaining = 0;
    }

    void setTargets(double leftMs, double rightMs, double headphonesMs, bool bypass) {
        m_requested = {
                framesForMs(leftMs), framesForMs(rightMs), framesForMs(headphonesMs)};
        m_requestedWet = !bypass;
    }

    void process(float* interleaved, std::size_t frames) {
        if (m_history.empty()) {
            return;
        }
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::size_t lane = 0; lane < m_lanes.size(); ++lane) {
                auto& state = m_lanes[lane];
                if (state.remaining == 0 && state.activeFrames != m_requested[lane]) {
                    state.fromFrames = state.activeFrames;
                    state.toFrames = m_requested[lane];
                    state.remaining = kTransitionFrames;
                }
            }
            if (m_targetWet != m_requestedWet) {
                m_bypassFromMix = m_wetMix;
                m_targetWet = m_requestedWet;
                m_bypassRemaining = kTransitionFrames;
            }
            if (m_bypassRemaining > 0) {
                const float fraction = static_cast<float>(
                                               kTransitionFrames - m_bypassRemaining + 1) /
                        kTransitionFrames;
                const float target = m_targetWet ? 1.0f : 0.0f;
                m_wetMix = m_bypassFromMix + (target - m_bypassFromMix) * fraction;
                --m_bypassRemaining;
                if (m_bypassRemaining == 0) {
                    m_wetMix = target;
                }
            }

            std::array<float, 4> dry{};
            const std::size_t frameBase = frame * 4;
            const std::size_t historyBase = static_cast<std::size_t>(m_writeFrame) * 4;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                dry[channel] = interleaved[frameBase + channel];
                m_history[historyBase + channel] = dry[channel];
            }
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const std::size_t lane = channel < 2 ? channel : 2;
                const auto& state = m_lanes[lane];
                const uint32_t fromFrames = state.remaining > 0
                        ? state.fromFrames
                        : state.activeFrames;
                const float from = delayedSample(fromFrames, channel);
                float wet = from;
                if (state.remaining > 0) {
                    const float to = delayedSample(state.toFrames, channel);
                    const float fraction = static_cast<float>(
                                                   kTransitionFrames - state.remaining + 1) /
                            kTransitionFrames;
                    wet = from + (to - from) * fraction;
                }
                if (m_wetMix <= 0.0f) {
                    interleaved[frameBase + channel] = dry[channel];
                } else if (m_wetMix >= 1.0f) {
                    interleaved[frameBase + channel] = wet;
                } else {
                    interleaved[frameBase + channel] =
                            dry[channel] + (wet - dry[channel]) * m_wetMix;
                }
            }
            for (auto& state : m_lanes) {
                if (state.remaining > 0 && --state.remaining == 0) {
                    state.activeFrames = state.toFrames;
                }
            }
            m_writeFrame = (m_writeFrame + 1) % kHistoryFrames;
        }
    }

  private:
    struct Lane {
        uint32_t activeFrames = 0;
        uint32_t fromFrames = 0;
        uint32_t toFrames = 0;
        uint32_t remaining = 0;
    };

    float delayedSample(uint32_t offset, std::size_t channel) const {
        const auto frame = (m_writeFrame + kHistoryFrames - offset) % kHistoryFrames;
        return m_history[static_cast<std::size_t>(frame) * 4 + channel];
    }

    std::vector<float> m_history;
    std::array<Lane, 3> m_lanes{};
    std::array<uint32_t, 3> m_requested{};
    uint32_t m_writeFrame = 0;
    bool m_requestedWet = true;
    bool m_targetWet = true;
    float m_wetMix = 1.0f;
    float m_bypassFromMix = 1.0f;
    uint32_t m_bypassRemaining = 0;
};

}
