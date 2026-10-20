#pragma once

#include <QJniObject>

#include "controllers/midi/androidmidistreamparser.h"
#include "controllers/midi/midicontroller.h"

class AndroidMidiController final : public MidiController {
    Q_OBJECT

  public:
    AndroidMidiController(const QJniObject& deviceInfo,
            int deviceInputPort,
            int deviceOutputPort,
            const QString& name,
            const QString& manufacturer,
            const QString& product,
            const QString& serial);
    ~AndroidMidiController() override;

    PhysicalTransportProtocol getPhysicalTransportProtocol() const override;
    QString getVendorString() const override;
    QString getProductString() const override;
    std::optional<uint16_t> getVendorId() const override;
    std::optional<uint16_t> getProductId() const override;
    QString getSerialNumber() const override;
    std::optional<uint8_t> getUsbInterfaceNumber() const override;
    bool sendBytes(const QByteArray& data) override;

  protected:
    void sendShortMsg(unsigned char status,
            unsigned char byte1,
            unsigned char byte2) override;

  private:
    int open(const QString& resourcePath) override;
    int close() override;
    bool poll() override;
    bool isPolling() const override;

    QJniObject m_bridge;
    QString m_manufacturer;
    QString m_product;
    QString m_serial;
    int m_deviceType;
    AndroidMidiStreamParser m_parser;
    jlong m_droppedPackets = 0;
    jlong m_epoch = -1;
};
