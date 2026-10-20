import QtQuick
import ".." as Skin

Button {
    id: root
    required property string group
    required property string key
    property bool toggleable: false
    highlight: behavior.isActive
    onPressed: behavior.pressPrimary()
    onReleased: behavior.releasePrimary()
    onCanceled: behavior.releasePrimary()
    Skin.ControlProxyButtonBehavior {
        id: behavior
        group: root.group
        key: root.key
        toggleable: root.toggleable
        handlePointerInput: false
    }
}
