#pragma once

#include <QtGlobal>

#ifdef Q_OS_ANDROID

#include <libusb.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QString>

#include "soundio/androidusb/pcmring.h"
#include "soundio/androidusb/outputdelay.h"
#include "soundio/androidusb/schedule.h"
#include "soundio/sounddevice.h"

class ControlObject;
class ControlPotmeter;
class QTimer;

class SoundDeviceAndroidUsbFlx6 final : public SoundDevice {
  public:
    static const QString kHostApi;

    SoundDeviceAndroidUsbFlx6(UserSettingsPointer config, SoundManager* manager);
    ~SoundDeviceAndroidUsbFlx6() override;

    SoundDeviceStatus open(bool isClkRefDevice, int syncBuffers) override;
    bool isOpen() const override;
    SoundDeviceStatus close() override;
    void readProcess(SINT framesPerBuffer) override;
    void writeProcess(SINT framesPerBuffer) override;
    QString getError() const override;
    mixxx::audio::SampleRate getDefaultSampleRate() const override;

  private:
    static constexpr int kPacketsPerTransfer = 8;
    static constexpr int kTransfers = 8;
    static constexpr int kMaximumPacketBytes = 576;
    static constexpr int kBytesPerFrame = 8;

    struct Slot {
        SoundDeviceAndroidUsbFlx6* owner = nullptr;
        libusb_transfer* transfer = nullptr;
        std::array<unsigned char, kPacketsPerTransfer * kMaximumPacketBytes> pcm{};
        bool active = false;
    };

    static void LIBUSB_CALL transferCallback(libusb_transfer* transfer);
    bool openUsb();
    bool submit(Slot& slot);
    void completed(Slot& slot);
    void eventLoop();
    void producerLoop();
    void stopAndDrain();
    void releaseUsb();
    void fail(const QString& message);
    void saveDelaySettings();

    int m_fd = -1;
    libusb_context* m_context = nullptr;
    libusb_device_handle* m_handle = nullptr;
    bool m_claimed = false;
    bool m_alternateSet = false;
    bool m_retainInFlight = false;
    std::array<std::unique_ptr<Slot>, kTransfers> m_slots;
    int m_inFlight = 0;
    std::atomic<bool> m_open{false};
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_primed{false};
    std::atomic<int> m_runtimeError{0};
    std::atomic<uint64_t> m_underflowFrames{0};
    std::atomic<uint64_t> m_completedFrames{0};
    std::thread m_producerThread;
    std::thread m_eventThread;
    std::mutex m_waitMutex;
    std::condition_variable m_waitCondition;
    std::vector<float> m_engineBuffer;
    mixxx::androidusb::PcmRing m_ring;
    std::unique_ptr<mixxx::androidusb::OutputDelay> m_outputDelay;
    probe::PacketSchedule m_schedule;
    QString m_lastError;
    std::unique_ptr<ControlPotmeter> m_pLeftDelayMs;
    std::unique_ptr<ControlPotmeter> m_pRightDelayMs;
    std::unique_ptr<ControlPotmeter> m_pHeadphonesDelayMs;
    std::unique_ptr<ControlPotmeter> m_pDelayBypass;
    std::unique_ptr<ControlObject> m_pDelayAvailable;
    std::unique_ptr<QTimer> m_pDelaySaveTimer;
};

#endif
