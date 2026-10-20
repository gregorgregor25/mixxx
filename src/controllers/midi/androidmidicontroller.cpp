#include "controllers/midi/androidmidicontroller.h"

#include <QJniArray>
#include <QtCore/private/qandroidextras_p.h>

#include "moc_androidmidicontroller.cpp"
#include "util/time.h"

AndroidMidiController::AndroidMidiController(const QJniObject& deviceInfo,
        int deviceInputPort,
        int deviceOutputPort,
        const QString& name,
        const QString& manufacturer,
        const QString& product,
        const QString& serial)
        : MidiController(name),
          m_manufacturer(manufacturer),
          m_product(product),
          m_serial(serial),
          m_deviceType(deviceInfo.callMethod<jint>("getType")) {
    setInputDevice(deviceOutputPort >= 0);
    setOutputDevice(deviceInputPort >= 0);
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    m_bridge = QJniObject("org/mixxx/AndroidMidiBridge",
            "(Landroid/content/Context;Landroid/media/midi/MidiDeviceInfo;II)V",
            context.object<jobject>(),
            deviceInfo.object<jobject>(),
            static_cast<jint>(deviceInputPort),
            static_cast<jint>(deviceOutputPort));
}

AndroidMidiController::~AndroidMidiController() {
    close();
}

PhysicalTransportProtocol AndroidMidiController::getPhysicalTransportProtocol() const {
    return m_deviceType == 1 ? PhysicalTransportProtocol::USB
                             : PhysicalTransportProtocol::UNKNOWN;
}

QString AndroidMidiController::getVendorString() const {
    return m_manufacturer;
}

QString AndroidMidiController::getProductString() const {
    return m_product;
}

std::optional<uint16_t> AndroidMidiController::getVendorId() const {
    return std::nullopt;
}

std::optional<uint16_t> AndroidMidiController::getProductId() const {
    return std::nullopt;
}

QString AndroidMidiController::getSerialNumber() const {
    return m_serial;
}

std::optional<uint8_t> AndroidMidiController::getUsbInterfaceNumber() const {
    return std::nullopt;
}

int AndroidMidiController::open(const QString& resourcePath) {
    if (isOpen() || !m_bridge.isValid()) {
        return -1;
    }
    m_parser.reset();
    m_epoch = -1;
    if (!m_bridge.callMethod<jboolean>("open", "(I)Z", 5000)) {
        qCWarning(m_logBase) << "Android MIDI device could not be opened:" << getName();
        return -1;
    }
    m_droppedPackets = m_bridge.callMethod<jlong>("getDroppedPacketCount");
    startEngine();
    setOpen(true);
    if (!applyMapping(resourcePath)) {
        qCWarning(m_logBase) << "Android MIDI controller mapping script did not initialize:"
                             << getName();
    }
    return 0;
}

int AndroidMidiController::close() {
    if (isOpen()) {
        stopEngine();
        MidiController::close();
        setOpen(false);
    }
    if (m_bridge.isValid()) {
        m_bridge.callMethod<void>("close");
    }
    m_parser.reset();
    return 0;
}

bool AndroidMidiController::poll() {
    if (!m_bridge.isValid() || !m_bridge.callMethod<jboolean>("isOpen")) {
        close();
        return false;
    }
    const jlong dropped = m_bridge.callMethod<jlong>("getDroppedPacketCount");
    if (dropped != m_droppedPackets) {
        qCWarning(m_logInput) << "Android MIDI input queue dropped"
                              << dropped - m_droppedPackets << "packets";
        m_droppedPackets = dropped;
        m_parser.reset();
    }

    bool received = false;
    for (int index = 0; index < 64; ++index) {
        QJniObject packet = m_bridge.callMethod<jobject>(
                "pollPacket", "()Lorg/mixxx/AndroidMidiBridge$Packet;");
        if (!packet.isValid()) {
            break;
        }
        QJniObject packetData = packet.callObjectMethod("getData", "()[B");
        QJniArray<jbyte> bytes(packetData.object<jbyteArray>());
        if (bytes.isEmpty()) {
            continue;
        }
        const jlong epoch = packet.callMethod<jlong>("getEpoch");
        if (epoch != m_epoch) {
            m_parser.reset();
            m_epoch = epoch;
        }
        QByteArray data = bytes.toContainer();
        const jlong packetNanos = packet.callMethod<jlong>("getTimestampNanos");
        const jlong nowNanos = QJniObject::callStaticMethod<jlong>(
                "java/lang/System", "nanoTime");
        mixxx::Duration timestamp = mixxx::Time::elapsed();
        const jlong ageNanos = nowNanos - packetNanos;
        if (packetNanos > 0 && ageNanos > 0 && ageNanos < 10000000000LL) {
            timestamp = timestamp - mixxx::Duration::fromNanos(ageNanos);
        }
        m_parser.feed(reinterpret_cast<const uint8_t*>(data.constData()),
                static_cast<std::size_t>(data.size()),
                [this, timestamp](uint8_t status, uint8_t byte1, uint8_t byte2) {
                    receivedShortMessage(status, byte1, byte2, timestamp);
                },
                [this, timestamp](const uint8_t* sysex, std::size_t size) {
                    receive(QByteArray(reinterpret_cast<const char*>(sysex),
                                    static_cast<qsizetype>(size)),
                            timestamp);
                });
        received = true;
    }
    return received;
}

bool AndroidMidiController::isPolling() const {
    return true;
}

void AndroidMidiController::sendShortMsg(unsigned char status,
        unsigned char byte1,
        unsigned char byte2) {
    if (!isOutputDevice()) {
        return;
    }
    QByteArray data;
    data.append(static_cast<char>(status));
    if (status < 0xF0) {
        data.append(static_cast<char>(byte1));
        if ((status & 0xF0) != 0xC0 && (status & 0xF0) != 0xD0) {
            data.append(static_cast<char>(byte2));
        }
    } else if (status == 0xF1 || status == 0xF3) {
        data.append(static_cast<char>(byte1));
    } else if (status == 0xF2) {
        data.append(static_cast<char>(byte1));
        data.append(static_cast<char>(byte2));
    }
    sendBytes(data);
}

bool AndroidMidiController::sendBytes(const QByteArray& data) {
    if (!m_bridge.isValid() || !isOutputDevice() || data.isEmpty()) {
        return false;
    }
    QJniArray<jbyte> bytes(data);
    return m_bridge.callMethod<jboolean>("send", "([B)Z", bytes.object<jbyteArray>());
}
