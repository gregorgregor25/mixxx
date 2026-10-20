#include "qmldlgpreferencesproxy.h"

#include <QMetaObject>

#include "moc_qmldlgpreferencesproxy.cpp"
#include "util/assert.h"

namespace mixxx {
namespace qml {

QmlDlgPreferencesProxy::QmlDlgPreferencesProxy(
        std::shared_ptr<QDialog> pDlgPreferences,
        QObject* parent)
        : QObject(parent),
          m_pDlgPreferences(pDlgPreferences) {
}

void QmlDlgPreferencesProxy::show() {
    QMetaObject::invokeMethod(
            m_pDlgPreferences.get(), "clearPageScope", Qt::DirectConnection);
    m_pDlgPreferences->show();
}

bool QmlDlgPreferencesProxy::showPage(const QString& pageId) {
    bool found = false;
    if (!QMetaObject::invokeMethod(m_pDlgPreferences.get(),
                "showPage",
                Qt::DirectConnection,
                Q_RETURN_ARG(bool, found),
                Q_ARG(QString, pageId)) ||
            !found) {
        return false;
    }
    m_pDlgPreferences->show();
    m_pDlgPreferences->raise();
    m_pDlgPreferences->activateWindow();
    return true;
}

bool QmlDlgPreferencesProxy::hasPage(const QString& pageId) const {
    bool found = false;
    return QMetaObject::invokeMethod(m_pDlgPreferences.get(),
                   "hasPage",
                   Qt::DirectConnection,
                   Q_RETURN_ARG(bool, found),
                   Q_ARG(QString, pageId)) &&
            found;
}

// static
QmlDlgPreferencesProxy* QmlDlgPreferencesProxy::create(
        QQmlEngine* pQmlEngine,
        QJSEngine* pJsEngine) {
    Q_UNUSED(pQmlEngine);

    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    DEBUG_ASSERT(s_pInstance);

    // The engine has to have the same thread affinity as the singleton.
    DEBUG_ASSERT(pJsEngine->thread() == s_pInstance->thread());

    // There can only be one engine accessing the singleton.
    if (s_pJsEngine) {
        DEBUG_ASSERT(pJsEngine == s_pJsEngine);
    } else {
        s_pJsEngine = pJsEngine;
    }

    // Explicitly specify C++ ownership so that the engine doesn't delete
    // the instance.
    QJSEngine::setObjectOwnership(s_pInstance.get(), QJSEngine::CppOwnership);
    return s_pInstance.get();
}

} // namespace qml
} // namespace mixxx
