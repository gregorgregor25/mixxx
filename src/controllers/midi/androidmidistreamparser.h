#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class AndroidMidiStreamParser {
  public:
    static constexpr std::size_t kMaxSysexBytes = 16384;

    void reset() {
        m_runningStatus = 0;
        m_pendingStatus = 0;
        m_dataCount = 0;
        m_expectedData = 0;
        m_inSysex = false;
        m_discardingSysex = false;
        m_sysex.clear();
    }

    template<typename ShortHandler, typename SysexHandler>
    void feed(const uint8_t* bytes,
            std::size_t size,
            ShortHandler&& shortHandler,
            SysexHandler&& sysexHandler) {
        for (std::size_t index = 0; index < size; ++index) {
            const uint8_t value = bytes[index];
            if (value >= 0xF8) {
                shortHandler(value, 0, 0);
                continue;
            }

            if (m_inSysex || m_discardingSysex) {
                if (value == 0xF7) {
                    if (m_inSysex) {
                        m_sysex.push_back(value);
                        sysexHandler(m_sysex.data(), m_sysex.size());
                    }
                    m_inSysex = false;
                    m_discardingSysex = false;
                    m_sysex.clear();
                    continue;
                }
                if (value < 0x80) {
                    if (m_inSysex) {
                        if (m_sysex.size() < kMaxSysexBytes - 1) {
                            m_sysex.push_back(value);
                        } else {
                            m_sysex.clear();
                            m_inSysex = false;
                            m_discardingSysex = true;
                        }
                    }
                    continue;
                }
                m_sysex.clear();
                m_inSysex = false;
                m_discardingSysex = false;
            }

            if (value & 0x80) {
                m_pendingStatus = 0;
                m_dataCount = 0;
                m_expectedData = 0;
                if (value == 0xF0) {
                    m_runningStatus = 0;
                    m_inSysex = true;
                    m_sysex.push_back(value);
                    continue;
                }
                if (value >= 0xF0) {
                    m_runningStatus = 0;
                    switch (value) {
                    case 0xF1:
                    case 0xF3:
                        m_expectedData = 1;
                        break;
                    case 0xF2:
                        m_expectedData = 2;
                        break;
                    case 0xF6:
                        shortHandler(value, 0, 0);
                        break;
                    default:
                        break;
                    }
                    if (m_expectedData) {
                        m_pendingStatus = value;
                    }
                    continue;
                }
                m_runningStatus = value;
                m_pendingStatus = value;
                m_expectedData = expectedDataBytes(value);
                continue;
            }

            if (!m_pendingStatus && m_runningStatus) {
                m_pendingStatus = m_runningStatus;
                m_expectedData = expectedDataBytes(m_runningStatus);
                m_dataCount = 0;
            }
            if (!m_pendingStatus) {
                continue;
            }
            m_data[m_dataCount++] = value;
            if (m_dataCount == m_expectedData) {
                shortHandler(m_pendingStatus,
                        m_data[0],
                        m_expectedData == 2 ? m_data[1] : 0);
                m_pendingStatus = 0;
                m_dataCount = 0;
                m_expectedData = 0;
            }
        }
    }

  private:
    static uint8_t expectedDataBytes(uint8_t status) {
        const uint8_t type = status & 0xF0;
        return type == 0xC0 || type == 0xD0 ? 1 : 2;
    }

    uint8_t m_runningStatus = 0;
    uint8_t m_pendingStatus = 0;
    uint8_t m_data[2] = {};
    uint8_t m_dataCount = 0;
    uint8_t m_expectedData = 0;
    bool m_inSysex = false;
    bool m_discardingSysex = false;
    std::vector<uint8_t> m_sysex;
};
