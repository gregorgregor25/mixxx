#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace probe {

struct AudioFormat {
    uint8_t configuration = 0;
    uint8_t interfaceNumber = 1;
    uint8_t alternateSetting = 1;
    uint8_t endpoint = 1;
    uint8_t endpointAttributes = 0;
    uint8_t interval = 0;
    uint8_t channels = 0;
    uint8_t subframeBytes = 0;
    uint8_t bitResolution = 0;
    uint16_t maxPacketBytes = 0;
    uint32_t sampleRate = 0;
    bool frequencyControl = false;
};

inline bool parseFlx6Format(const uint8_t* bytes,
        size_t length,
        AudioFormat* output,
        std::string* error) {
    auto fail = [error](const char* message) {
        *error = message;
        return false;
    };
    if (length < 27 || bytes[0] < 18 || bytes[1] != 1 ||
            bytes[8] != 0x73 || bytes[9] != 0x2b ||
            bytes[10] != 0x38 || bytes[11] != 0x00) {
        return fail("Raw device descriptor is not DDJ-FLX6 (2b73:0038)");
    }
    size_t configStart = 0;
    for (size_t i = bytes[0]; i + 2 <= length;) {
        const uint8_t descriptorLength = bytes[i];
        if (descriptorLength < 2 || i + descriptorLength > length) {
            return fail("Malformed raw descriptor length");
        }
        if (bytes[i + 1] == 2) {
            configStart = i;
            break;
        }
        i += descriptorLength;
    }
    if (configStart == 0 || length - configStart < 9 ||
            bytes[configStart] < 9) {
        return fail("Raw configuration descriptor missing");
    }
    const size_t configLength = bytes[configStart + 2] |
            (static_cast<size_t>(bytes[configStart + 3]) << 8);
    if (configLength < 9 || configStart + configLength > length) {
        return fail("Raw configuration descriptor is truncated");
    }
    AudioFormat format;
    format.configuration = bytes[configStart + 5];
    if (format.configuration != 1) {
        return fail("Unexpected configuration value");
    }
    bool inTarget = false;
    bool targetFound = false;
    bool generalFound = false;
    bool formatFound = false;
    bool endpointFound = false;
    const size_t end = configStart + configLength;
    for (size_t i = configStart + bytes[configStart]; i < end;) {
        if (i + 2 > end || bytes[i] < 2 || i + bytes[i] > end) {
            return fail("Malformed configuration child descriptor");
        }
        const uint8_t* item = bytes + i;
        const uint8_t descriptorLength = item[0];
        const uint8_t type = item[1];
        if (type == 4) {
            if (descriptorLength < 9) {
                return fail("Short interface descriptor");
            }
            inTarget = item[2] == 1 && item[3] == 1;
            if (inTarget) {
                if (targetFound || item[4] != 1 || item[5] != 1 ||
                        item[6] != 2 || item[7] != 0) {
                    return fail("Unexpected or repeated audio streaming alternate setting");
                }
                targetFound = true;
            }
        } else if (inTarget && type == 0x24 && descriptorLength >= 3 && item[2] == 1) {
            if (generalFound || descriptorLength < 7 || item[5] != 1 || item[6] != 0) {
                return fail("Missing or non-PCM UAC1 AS_GENERAL descriptor");
            }
            generalFound = true;
        } else if (inTarget && type == 0x24 && descriptorLength >= 3 && item[2] == 2) {
            if (formatFound || descriptorLength < 11 || item[3] != 1 || item[4] != 4 ||
                    item[5] != 2 || item[6] != 16 || item[7] != 1) {
                return fail("Unsupported FLX6 UAC1 format");
            }
            format.sampleRate = item[8] |
                    (static_cast<uint32_t>(item[9]) << 8) |
                    (static_cast<uint32_t>(item[10]) << 16);
            format.channels = item[4];
            format.subframeBytes = item[5];
            format.bitResolution = item[6];
            formatFound = true;
        } else if (inTarget && type == 5) {
            if (descriptorLength < 7 || endpointFound) {
                return fail("Missing or multiple streaming endpoints");
            }
            format.endpoint = item[2];
            format.endpointAttributes = item[3];
            format.maxPacketBytes = item[4] | (static_cast<uint16_t>(item[5]) << 8);
            format.interval = item[6];
            endpointFound = true;
        } else if (inTarget && type == 0x25 && descriptorLength >= 4 && item[2] == 1) {
            format.frequencyControl = (item[3] & 1) != 0;
        }
        i += descriptorLength;
    }
    if (!targetFound || !generalFound || !formatFound || !endpointFound ||
            format.sampleRate != 44100 || format.endpoint != 0x01 ||
            (format.endpointAttributes & 0x0f) != 0x0d ||
            format.maxPacketBytes != 576 ||
            format.interval != 4) {
        return fail("FLX6 interface 1 alt 1 is not the expected 44.1 kHz four-channel ISO OUT format");
    }
    *output = format;
    return true;
}

}
