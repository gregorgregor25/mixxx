#pragma once

#include <cstdint>

namespace probe {

struct PacketSchedule {
    uint32_t sampleRate = 44100;
    uint32_t intervalUs = 1000;
    uint32_t accumulator = 0;

    uint32_t nextFrames() {
        accumulator += sampleRate * intervalUs;
        const uint32_t frames = accumulator / 1000000;
        accumulator %= 1000000;
        return frames;
    }
};

struct CompletionCounts {
    uint64_t callbacks = 0;
    uint64_t packetSuccess = 0;
    uint64_t packetFailure = 0;
    uint64_t successfulRequestedBytes = 0;
    uint64_t reportedActualBytes = 0;
    uint64_t successfulFrames = 0;

    void addPacket(bool success, uint32_t requested, uint32_t actual, uint32_t bytesPerFrame) {
        if (success && actual <= requested && requested % bytesPerFrame == 0) {
            ++packetSuccess;
            successfulRequestedBytes += requested;
            reportedActualBytes += actual;
            successfulFrames += requested / bytesPerFrame;
        } else {
            ++packetFailure;
        }
    }
};

}
