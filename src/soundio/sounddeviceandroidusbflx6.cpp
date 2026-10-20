#include "soundio/sounddeviceandroidusbflx6.h"

#ifdef Q_OS_ANDROID

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <utility>

#include <QJniArray>
#include <QJniObject>
#include <QDebug>
#include <QTimer>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "soundio/androidusb/descriptors.h"
#include "soundio/soundmanager.h"
#include "util/defs.h"

namespace {

constexpr int kRuntimeBadPacket = -1000;
constexpr int kRuntimeDrainTimeout = -1001;
const QString kDelayGroup = QStringLiteral("[Flx6OutputDelay]");

struct OpenedUsb {
    int fd = -1;
    std::vector<uint8_t> descriptors;
    QString error;
};

OpenedUsb openFromAndroid() {
    OpenedUsb result;
    QJniObject bridge = QJniObject::callStaticObjectMethod(
            "org/mixxx/AndroidUsbAudioBridge",
            "openFlx6",
            "()Lorg/mixxx/AndroidUsbAudioBridge;");
    if (!bridge.isValid()) {
        result.error = QJniObject::callStaticObjectMethod(
                               "org/mixxx/AndroidUsbAudioBridge",
                               "lastError",
                               "()Ljava/lang/String;")
                               .toString();
        if (result.error.isEmpty()) {
            result.error = QStringLiteral("Unable to open FLX6 with Android UsbManager");
        }
        return result;
    }
    const auto fd = bridge.callMethod<jint>("getFileDescriptor");
    QJniObject rawObject = bridge.callObjectMethod("getRawDescriptors", "()[B");
    QJniArray<jbyte> raw(rawObject.object<jbyteArray>());
    if (fd >= 0 && raw.size() >= 27 && raw.size() <= 4096) {
        result.descriptors.reserve(raw.size());
        for (jbyte byte : raw) {
            result.descriptors.push_back(static_cast<uint8_t>(byte));
        }
        result.fd = ::dup(fd);
    }
    bridge.callMethod<void>("close");
    if (result.fd < 0) {
        result.error = QStringLiteral("FLX6 USB descriptor or file descriptor is unavailable");
    }
    return result;
}

}

const QString SoundDeviceAndroidUsbFlx6::kHostApi =
        QStringLiteral("Android FLX6 Direct USB");

SoundDeviceAndroidUsbFlx6::SoundDeviceAndroidUsbFlx6(
        UserSettingsPointer config, SoundManager* manager)
        : SoundDevice(std::move(config), manager) {
    m_deviceId.name = QStringLiteral("DDJ-FLX6 (Direct USB)");
    m_deviceId.deviceIndex = -1;
    m_strDisplayName = m_deviceId.name;
    m_hostAPI = kHostApi;
    m_numInputChannels = mixxx::audio::ChannelCount(0);
    m_numOutputChannels = mixxx::audio::ChannelCount(4);
    auto persistentDelay = [](const QString& key) {
        auto control = std::make_unique<ControlPotmeter>(
                ConfigKey(kDelayGroup, key), 0.0, 1000.0, false, true, false, true, 0.0);
        control->setDefaultValue(0.0);
        return control;
    };
    m_pLeftDelayMs = persistentDelay(QStringLiteral("left_ms"));
    m_pRightDelayMs = persistentDelay(QStringLiteral("right_ms"));
    m_pHeadphonesDelayMs = persistentDelay(QStringLiteral("headphones_ms"));
    m_pDelayBypass = std::make_unique<ControlPotmeter>(
            ConfigKey(kDelayGroup, QStringLiteral("bypass")),
            0.0, 1.0, false, true, false, true, 0.0);
    m_pDelayBypass->setDefaultValue(0.0);
    m_pLeftDelayMs->forceSet(
            mixxx::androidusb::OutputDelay::normalizedMs(m_pLeftDelayMs->get()));
    m_pRightDelayMs->forceSet(
            mixxx::androidusb::OutputDelay::normalizedMs(m_pRightDelayMs->get()));
    m_pHeadphonesDelayMs->forceSet(
            mixxx::androidusb::OutputDelay::normalizedMs(m_pHeadphonesDelayMs->get()));
    m_pDelayBypass->forceSet(
            mixxx::androidusb::OutputDelay::normalizedMs(m_pDelayBypass->get()) != 0);
    m_pDelayAvailable = std::make_unique<ControlObject>(
            ConfigKey(kDelayGroup, QStringLiteral("available")), true, false, false, 1.0);
    m_pDelayAvailable->setReadOnly();
    m_pDelaySaveTimer = std::make_unique<QTimer>();
    m_pDelaySaveTimer->setSingleShot(true);
    m_pDelaySaveTimer->setInterval(300);
    QObject::connect(m_pDelaySaveTimer.get(), &QTimer::timeout,
            m_pDelaySaveTimer.get(), [this] { saveDelaySettings(); });
    auto scheduleSave = [this](ControlPotmeter* control) {
        QObject::connect(control, &ControlObject::valueChanged,
                m_pDelaySaveTimer.get(), [this](double) {
                    m_pDelaySaveTimer->start();
                }, Qt::QueuedConnection);
    };
    scheduleSave(m_pLeftDelayMs.get());
    scheduleSave(m_pRightDelayMs.get());
    scheduleSave(m_pHeadphonesDelayMs.get());
    scheduleSave(m_pDelayBypass.get());
}

SoundDeviceAndroidUsbFlx6::~SoundDeviceAndroidUsbFlx6() {
    close();
    if (m_pDelaySaveTimer) {
        m_pDelaySaveTimer->stop();
    }
    saveDelaySettings();
}

SoundDeviceStatus SoundDeviceAndroidUsbFlx6::open(bool isClkRefDevice, int syncBuffers) {
    Q_UNUSED(syncBuffers);
    if (m_retainInFlight) {
        m_lastError = QStringLiteral("FLX6 USB transfers did not drain; restart Mixxx before reopening");
        return SoundDeviceStatus::Error;
    }
    if (isOpen()) {
        m_lastError = QStringLiteral("FLX6 direct USB is already open");
        return SoundDeviceStatus::Error;
    }
    m_lastError.clear();
    if (!isClkRefDevice || m_audioOutputs.isEmpty() ||
            !m_audioInputs.isEmpty()) {
        m_lastError = QStringLiteral("FLX6 direct USB requires one output clock and no audio inputs");
        return SoundDeviceStatus::Error;
    }
    if (m_sampleRate != 44100 || m_configFramesPerBuffer < 64 ||
            m_configFramesPerBuffer > kMaxEngineFrames) {
        m_lastError = QStringLiteral("FLX6 direct USB requires 44.1 kHz and a valid audio buffer");
        return SoundDeviceStatus::Error;
    }
    m_engineBuffer.resize(static_cast<std::size_t>(m_configFramesPerBuffer) *
            mixxx::androidusb::kChannels);
    m_outputDelay = std::make_unique<mixxx::androidusb::OutputDelay>();
    m_outputDelay->prepare(m_pLeftDelayMs->get(), m_pRightDelayMs->get(),
            m_pHeadphonesDelayMs->get(),
            mixxx::androidusb::OutputDelay::normalizedMs(m_pDelayBypass->get()) != 0);
    m_ring.reset();
    m_stop.store(false);
    m_primed.store(false);
    m_runtimeError.store(0);
    m_underflowFrames.store(0);
    m_completedFrames.store(0);
    m_schedule = probe::PacketSchedule{};
    if (!openUsb()) {
        releaseUsb();
        m_outputDelay.reset();
        return SoundDeviceStatus::Error;
    }
    m_open.store(true);
    m_producerThread = std::thread([this] { producerLoop(); });
    {
        std::unique_lock<std::mutex> lock(m_waitMutex);
        if (!m_waitCondition.wait_for(lock, std::chrono::seconds(3), [this] {
                return m_primed.load() || m_stop.load();
            }) || !m_primed.load()) {
            m_lastError = QStringLiteral("FLX6 Mixxx engine did not prime USB audio in time");
            lock.unlock();
            close();
            return SoundDeviceStatus::Error;
        }
    }
    m_eventThread = std::thread([this] { eventLoop(); });
    qInfo() << "FLX6 direct USB audio opened at 44100 Hz, four 16-bit channels"
            << "Mixxx block frames" << m_configFramesPerBuffer
            << "USB queued transfers" << kTransfers;
    return SoundDeviceStatus::Ok;
}

bool SoundDeviceAndroidUsbFlx6::isOpen() const {
    return m_open.load();
}

SoundDeviceStatus SoundDeviceAndroidUsbFlx6::close() {
    if (!m_open.load() && !m_producerThread.joinable() &&
            !m_eventThread.joinable()) {
        return SoundDeviceStatus::Ok;
    }
    m_stop.store(true);
    m_waitCondition.notify_all();
    if (m_producerThread.joinable()) {
        m_producerThread.join();
    }
    if (m_eventThread.joinable()) {
        m_eventThread.join();
    }
    releaseUsb();
    m_outputDelay.reset();
    m_open.store(false);
    qInfo() << "FLX6 direct USB frames completed" << m_completedFrames.load()
            << "underflow frames" << m_underflowFrames.load();
    m_pDelaySaveTimer->stop();
    saveDelaySettings();
    return SoundDeviceStatus::Ok;
}

void SoundDeviceAndroidUsbFlx6::readProcess(SINT) {
}

void SoundDeviceAndroidUsbFlx6::writeProcess(SINT) {
}

QString SoundDeviceAndroidUsbFlx6::getError() const {
    const int runtime = m_runtimeError.load();
    if (runtime == kRuntimeBadPacket) {
        return QStringLiteral("FLX6 USB audio packet failed or device detached");
    }
    if (runtime == kRuntimeDrainTimeout) {
        return QStringLiteral("FLX6 USB transfers did not drain on shutdown");
    }
    if (runtime != 0) {
        return QStringLiteral("FLX6 USB audio transport failed: %1")
                .arg(QString::fromLatin1(libusb_error_name(runtime)));
    }
    return m_lastError;
}

mixxx::audio::SampleRate SoundDeviceAndroidUsbFlx6::getDefaultSampleRate() const {
    return mixxx::audio::SampleRate(44100);
}

bool SoundDeviceAndroidUsbFlx6::openUsb() {
    OpenedUsb opened = std::async(std::launch::async, openFromAndroid).get();
    if (opened.fd < 0) {
        fail(opened.error);
        return false;
    }
    m_fd = opened.fd;
    probe::AudioFormat format;
    std::string descriptorError;
    if (!probe::parseFlx6Format(opened.descriptors.data(),
                opened.descriptors.size(), &format, &descriptorError)) {
        fail(QString::fromStdString(descriptorError));
        return false;
    }
    libusb_init_option option{};
    option.option = LIBUSB_OPTION_NO_DEVICE_DISCOVERY;
    int rc = libusb_init_context(&m_context, &option, 1);
    if (rc != LIBUSB_SUCCESS) {
        fail(QStringLiteral("libusb_init: %1")
                        .arg(QString::fromLatin1(libusb_error_name(rc))));
        return false;
    }
    rc = libusb_wrap_sys_device(m_context, m_fd, &m_handle);
    if (rc != LIBUSB_SUCCESS) {
        fail(QStringLiteral("libusb_wrap_sys_device: %1")
                        .arg(QString::fromLatin1(libusb_error_name(rc))));
        return false;
    }
    libusb_device* device = libusb_get_device(m_handle);
    libusb_device_descriptor identity{};
    rc = libusb_get_device_descriptor(device, &identity);
    if (rc != LIBUSB_SUCCESS || identity.idVendor != 0x2b73 ||
            identity.idProduct != 0x0038 ||
            libusb_get_device_speed(device) != LIBUSB_SPEED_HIGH) {
        fail(QStringLiteral("FLX6 USB identity or high-speed mode was not verified"));
        return false;
    }
    if (125 * (1u << (format.interval - 1)) != 1000) {
        fail(QStringLiteral("FLX6 audio endpoint does not use 1 ms packets"));
        return false;
    }
    int configuration = 0;
    rc = libusb_get_configuration(m_handle, &configuration);
    if (rc != LIBUSB_SUCCESS || configuration != format.configuration) {
        fail(QStringLiteral("FLX6 is not in USB configuration 1"));
        return false;
    }
    libusb_config_descriptor* active = nullptr;
    rc = libusb_get_active_config_descriptor(device, &active);
    if (rc != LIBUSB_SUCCESS || active == nullptr) {
        fail(QStringLiteral("Unable to read FLX6 active USB configuration"));
        return false;
    }
    bool matchingAlternate = false;
    for (int i = 0; i < active->bNumInterfaces; ++i) {
        const auto& interface = active->interface[i];
        for (int alt = 0; alt < interface.num_altsetting; ++alt) {
            const auto& setting = interface.altsetting[alt];
            if (setting.bInterfaceNumber == 1 && setting.bAlternateSetting == 1 &&
                    setting.bInterfaceClass == LIBUSB_CLASS_AUDIO &&
                    setting.bInterfaceSubClass == 2 && setting.bInterfaceProtocol == 0 &&
                    setting.bNumEndpoints == 1 &&
                    setting.endpoint[0].bEndpointAddress == 0x01 &&
                    (setting.endpoint[0].bmAttributes & 0x0f) == 0x0d &&
                    setting.endpoint[0].wMaxPacketSize == 576 &&
                    setting.endpoint[0].bInterval == 4) {
                matchingAlternate = true;
            }
        }
    }
    libusb_free_config_descriptor(active);
    if (!matchingAlternate) {
        fail(QStringLiteral("FLX6 active USB audio descriptor changed"));
        return false;
    }
    libusb_set_auto_detach_kernel_driver(m_handle, 1);
    rc = libusb_claim_interface(m_handle, 1);
    if (rc != LIBUSB_SUCCESS) {
        fail(QStringLiteral("FLX6 audio interface claim failed: %1")
                        .arg(QString::fromLatin1(libusb_error_name(rc))));
        return false;
    }
    m_claimed = true;
    rc = libusb_set_interface_alt_setting(m_handle, 1, 1);
    if (rc != LIBUSB_SUCCESS) {
        fail(QStringLiteral("FLX6 audio alternate setting failed: %1")
                        .arg(QString::fromLatin1(libusb_error_name(rc))));
        return false;
    }
    m_alternateSet = true;
    if (format.frequencyControl) {
        unsigned char rate[3] = {0x44, 0xac, 0x00};
        rc = libusb_control_transfer(m_handle, 0x22, 0x01, 0x0100, 0x0001,
                rate, 3, 1000);
        if (rc != 3) {
            fail(QStringLiteral("FLX6 44.1 kHz endpoint request failed"));
            return false;
        }
    }
    for (auto& slot : m_slots) {
        slot = std::make_unique<Slot>();
        slot->owner = this;
        slot->transfer = libusb_alloc_transfer(kPacketsPerTransfer);
        if (!slot->transfer) {
            fail(QStringLiteral("Unable to allocate FLX6 USB ISO transfers"));
            return false;
        }
    }
    return true;
}

void LIBUSB_CALL SoundDeviceAndroidUsbFlx6::transferCallback(libusb_transfer* transfer) {
    auto* slot = static_cast<Slot*>(transfer->user_data);
    if (slot->owner) {
        slot->owner->completed(*slot);
    }
}

bool SoundDeviceAndroidUsbFlx6::submit(Slot& slot) {
    int totalBytes = 0;
    for (int packet = 0; packet < kPacketsPerTransfer; ++packet) {
        const int frames = static_cast<int>(m_schedule.nextFrames());
        const int bytes = frames * kBytesPerFrame;
        if (bytes <= 0 || bytes > kMaximumPacketBytes) {
            m_runtimeError.store(kRuntimeBadPacket);
            m_stop.store(true);
            return false;
        }
        const auto copied = m_ring.popBytes(slot.pcm.data() + totalBytes, frames);
        if (copied < static_cast<std::size_t>(frames)) {
            m_underflowFrames.fetch_add(frames - copied);
        }
        slot.transfer->iso_packet_desc[packet].length = bytes;
        totalBytes += bytes;
    }
    m_waitCondition.notify_one();
    libusb_fill_iso_transfer(slot.transfer, m_handle, 0x01, slot.pcm.data(),
            totalBytes, kPacketsPerTransfer, transferCallback, &slot, 0);
    const int rc = libusb_submit_transfer(slot.transfer);
    if (rc != LIBUSB_SUCCESS) {
        m_runtimeError.store(rc);
        m_stop.store(true);
        m_waitCondition.notify_one();
        return false;
    }
    slot.active = true;
    ++m_inFlight;
    return true;
}

void SoundDeviceAndroidUsbFlx6::completed(Slot& slot) {
    slot.active = false;
    --m_inFlight;
    if (slot.transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        if (slot.transfer->status != LIBUSB_TRANSFER_CANCELLED) {
            m_runtimeError.store(kRuntimeBadPacket);
            m_stop.store(true);
            m_waitCondition.notify_one();
        }
        return;
    }
    for (int packet = 0; packet < slot.transfer->num_iso_packets; ++packet) {
        const auto& result = slot.transfer->iso_packet_desc[packet];
        if (result.status != LIBUSB_TRANSFER_COMPLETED ||
                result.actual_length != result.length ||
                result.length % kBytesPerFrame != 0) {
            m_runtimeError.store(kRuntimeBadPacket);
            m_stop.store(true);
            m_waitCondition.notify_one();
            return;
        }
        m_completedFrames.fetch_add(result.length / kBytesPerFrame);
    }
    if (!m_stop.load()) {
        submit(slot);
    }
}

void SoundDeviceAndroidUsbFlx6::eventLoop() {
    for (auto& slot : m_slots) {
        if (m_stop.load() || !submit(*slot)) {
            break;
        }
    }
    while (!m_stop.load()) {
        timeval timeout{0, 50000};
        const int rc = libusb_handle_events_timeout_completed(m_context, &timeout, nullptr);
        if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_INTERRUPTED) {
            m_runtimeError.store(rc);
            m_stop.store(true);
            m_waitCondition.notify_one();
        }
    }
    stopAndDrain();
}

void SoundDeviceAndroidUsbFlx6::producerLoop() {
    const auto frames = static_cast<std::size_t>(m_configFramesPerBuffer);
    const auto prime = std::max(frames,
            static_cast<std::size_t>(kTransfers * kPacketsPerTransfer * 45));
    while (!m_stop.load()) {
        if (m_ring.availableFrames() >= prime || m_ring.freeFrames() < frames) {
            std::unique_lock<std::mutex> lock(m_waitMutex);
            m_waitCondition.wait_for(lock, std::chrono::milliseconds(20), [this, prime, frames] {
                return m_stop.load() || (m_ring.availableFrames() < prime &&
                        m_ring.freeFrames() >= frames);
            });
            continue;
        }
        m_pSoundManager->processUnderflowHappened(static_cast<SINT>(frames));
        m_pSoundManager->readProcess(static_cast<SINT>(frames));
        m_pSoundManager->onDeviceOutputCallback(static_cast<SINT>(frames));
        composeOutputBuffer(m_engineBuffer.data(), static_cast<SINT>(frames), 0,
                static_cast<int>(mixxx::androidusb::kChannels));
        m_outputDelay->setTargets(m_pLeftDelayMs->get(), m_pRightDelayMs->get(),
                m_pHeadphonesDelayMs->get(),
                mixxx::androidusb::OutputDelay::normalizedMs(m_pDelayBypass->get()) != 0);
        m_outputDelay->process(m_engineBuffer.data(), frames);
        m_pSoundManager->writeProcess(static_cast<SINT>(frames));
        if (!m_ring.pushFloat(m_engineBuffer.data(), frames)) {
            m_runtimeError.store(kRuntimeBadPacket);
            m_stop.store(true);
            m_waitCondition.notify_all();
            return;
        }
        if (!m_primed.load() && m_ring.availableFrames() >= prime) {
            m_primed.store(true);
            m_waitCondition.notify_all();
        }
    }
}

void SoundDeviceAndroidUsbFlx6::stopAndDrain() {
    for (auto& slot : m_slots) {
        if (slot->active) {
            libusb_cancel_transfer(slot->transfer);
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (m_inFlight > 0 && std::chrono::steady_clock::now() < deadline) {
        timeval timeout{0, 50000};
        libusb_handle_events_timeout_completed(m_context, &timeout, nullptr);
    }
    if (m_inFlight > 0) {
        m_runtimeError.store(kRuntimeDrainTimeout);
        m_retainInFlight = true;
        for (auto& slot : m_slots) {
            slot->owner = nullptr;
            slot.release();
        }
    }
}

void SoundDeviceAndroidUsbFlx6::releaseUsb() {
    if (m_retainInFlight) {
        qWarning() << "FLX6 in-flight USB resources retained after drain timeout";
        return;
    }
    for (auto& slot : m_slots) {
        if (slot && slot->transfer) {
            libusb_free_transfer(slot->transfer);
            slot->transfer = nullptr;
        }
        slot.reset();
    }
    if (m_claimed) {
        if (m_alternateSet) {
            libusb_set_interface_alt_setting(m_handle, 1, 0);
            m_alternateSet = false;
        }
        libusb_release_interface(m_handle, 1);
        m_claimed = false;
    }
    if (m_handle) {
        libusb_close(m_handle);
        m_handle = nullptr;
    }
    if (m_context) {
        libusb_exit(m_context);
        m_context = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void SoundDeviceAndroidUsbFlx6::fail(const QString& message) {
    m_lastError = message;
    qWarning() << "FLX6 direct USB:" << message;
}

void SoundDeviceAndroidUsbFlx6::saveDelaySettings() {
    if (!m_pConfig || !m_pLeftDelayMs || !m_pRightDelayMs ||
            !m_pHeadphonesDelayMs || !m_pDelayBypass) {
        return;
    }
    const auto left = mixxx::androidusb::OutputDelay::normalizedMs(m_pLeftDelayMs->get());
    const auto right = mixxx::androidusb::OutputDelay::normalizedMs(m_pRightDelayMs->get());
    const auto headphones =
            mixxx::androidusb::OutputDelay::normalizedMs(m_pHeadphonesDelayMs->get());
    const unsigned int bypass =
            mixxx::androidusb::OutputDelay::normalizedMs(m_pDelayBypass->get()) != 0;
    m_pLeftDelayMs->forceSet(left);
    m_pRightDelayMs->forceSet(right);
    m_pHeadphonesDelayMs->forceSet(headphones);
    m_pDelayBypass->forceSet(bypass);
    m_pConfig->setValue(ConfigKey(kDelayGroup, QStringLiteral("left_ms")), left);
    m_pConfig->setValue(ConfigKey(kDelayGroup, QStringLiteral("right_ms")), right);
    m_pConfig->setValue(ConfigKey(kDelayGroup, QStringLiteral("headphones_ms")), headphones);
    m_pConfig->setValue(ConfigKey(kDelayGroup, QStringLiteral("bypass")), bypass);
    if (!m_pConfig->save()) {
        qWarning() << "Unable to save FLX6 output delay settings";
    }
}

#endif
