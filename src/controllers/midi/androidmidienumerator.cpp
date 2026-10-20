#include "controllers/midi/androidmidienumerator.h"

#include <QJniArray>
#include <QJniObject>
#include <QtAlgorithms>
#include <QtCore/private/qandroidextras_p.h>

#include "controllers/midi/androidmidicontroller.h"
#include "moc_androidmidienumerator.cpp"

namespace {
QString deviceProperty(const QJniObject& info, const char* propertyName) {
    QJniObject key = QJniObject::getStaticObjectField(
            "android/media/midi/MidiDeviceInfo",
            propertyName,
            "Ljava/lang/String;");
    if (!key.isValid()) {
        return {};
    }
    QJniObject value = QJniObject::callStaticObjectMethod(
            "org/mixxx/AndroidMidiBridge",
            "property",
            "(Landroid/media/midi/MidiDeviceInfo;Ljava/lang/String;)Ljava/lang/String;",
            info.object<jobject>(),
            key.object<jstring>());
    return value.toString();
}
}

AndroidMidiEnumerator::AndroidMidiEnumerator() = default;

AndroidMidiEnumerator::~AndroidMidiEnumerator() {
    qDeleteAll(m_devices);
}

QList<Controller*> AndroidMidiEnumerator::queryDevices() {
    if (m_queried) {
        return m_devices;
    }
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        qWarning() << "Android MIDI enumeration has no context";
        return m_devices;
    }
    QJniObject infos = QJniObject::callStaticObjectMethod(
            "org/mixxx/AndroidMidiBridge",
            "enumerate",
            "(Landroid/content/Context;)[Landroid/media/midi/MidiDeviceInfo;",
            context.object<jobject>());
    if (!infos.isValid()) {
        qWarning() << "Android MIDI device list is unavailable";
        return m_devices;
    }
    m_queried = true;

    QJniArray<QJniObject> devices(infos.object<jobjectArray>());
    for (const QJniObject& info : devices) {
        const int deviceId = info.callMethod<jint>("getId");
        const int deviceInputCount = info.callMethod<jint>("getInputPortCount");
        const int deviceOutputCount = info.callMethod<jint>("getOutputPortCount");
        if (deviceOutputCount == 0) {
            qInfo() << "Skipping Android MIDI device without a hardware output port"
                    << deviceId;
            continue;
        }

        QString manufacturer = deviceProperty(info, "PROPERTY_MANUFACTURER");
        QString product = deviceProperty(info, "PROPERTY_PRODUCT");
        QString name = !product.isEmpty() ? product
                : deviceProperty(info, "PROPERTY_NAME");
        QString serial = deviceProperty(info, "PROPERTY_SERIAL_NUMBER");
        if (name.isEmpty()) {
            name = QStringLiteral("Android MIDI %1").arg(deviceId);
        }
        if (product.isEmpty()) {
            product = name;
        }

        for (int outputPort = 0; outputPort < deviceOutputCount; ++outputPort) {
            const int inputPort = outputPort < deviceInputCount ? outputPort : -1;
            QString controllerName = name;
            if (deviceInputCount > 1 || deviceOutputCount > 1) {
                controllerName += QStringLiteral(" MIDI %1").arg(outputPort + 1);
            }
            qInfo() << "Found Android MIDI controller" << controllerName
                    << "deviceId" << deviceId
                    << "device output / Mixxx input" << outputPort
                    << "device input / Mixxx output" << inputPort;
            m_devices.push_back(new AndroidMidiController(info,
                    inputPort,
                    outputPort,
                    controllerName,
                    manufacturer,
                    product,
                    serial));
        }
    }
    return m_devices;
}
