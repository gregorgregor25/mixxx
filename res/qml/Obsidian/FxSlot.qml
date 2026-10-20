pragma ComponentBehavior: Bound

import "." as Studio
import "../Effects" as Effects
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property int unitNumber
    required property int effectNumber
    required property color accent
    readonly property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(unitNumber, effectNumber)

    implicitHeight: 34
    Mixxx.ControlProxy { id: parameterValue; group: root.slot.group; key: "fx_parameter_value" }
    Mixxx.ControlProxy { id: parameterType; group: root.slot.group; key: "fx_parameter_type" }
    Mixxx.ControlProxy { id: controllerSelected; group: root.slot.group; key: "controller_selected" }
    readonly property string parameterText: parameterType.value === 2
            ? Math.round(parameterValue.value) + "%"
            : parameterType.value === 1 ? formatBeats(parameterValue.value) : "—"

    function formatBeats(value) {
        if (!Number.isFinite(value) || value <= 0)
            return "—";
        for (const denominator of [1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 96]) {
            const numerator = Math.round(value * denominator);
            if (numerator > 0 && Math.abs(value - numerator / denominator) < 0.0001)
                return denominator === 1 ? String(numerator) : numerator + "/" + denominator;
        }
        return value.toFixed(2);
    }

    Column {
        id: beatReadout
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 39
        spacing: 1
        opacity: parameterType.value > 0 ? 1 : 0.5
        Accessible.role: Accessible.StaticText
        Accessible.name: parameterType.value > 0
                ? root.slot.effectName + (parameterType.value === 2 ? ": decay " : ": selected beat setting ") + root.parameterText
                : "No separate arrow parameter for this effect"

        Text {
            width: parent.width
            text: root.parameterText
            color: root.accent
            font.pixelSize: 12
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 9
        }
        Text {
            width: parent.width
            text: parameterType.value === 2 ? "DECAY" : parameterType.value === 1 ? "BEAT SET" : "NO PARAM"
            color: "#9ba8b2"
            font.pixelSize: 7
            horizontalAlignment: Text.AlignHCenter
        }
        HoverHandler { id: beatHover }
        ToolTip.visible: beatHover.hovered
        ToolTip.text: parameterType.value === 2
                ? "Reverb decay. The controller arrows adjust this in 5% steps."
                : parameterType.value === 1
                    ? "Selected beat setting. Without detected tempo, this effect uses its time-based fallback."
                    : "This effect has no separate parameter assigned to the controller arrows."
    }

    Studio.ControlButton {
        id: enableButton
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 37
        height: 32
        group: root.slot.group
        key: "enabled"
        text: String(root.effectNumber)
        toggleable: true
        accent: root.accent
        borderColor: controllerSelected.value > 0 || activeFocus ? root.accent : "#4b5055"
        borderWidth: controllerSelected.value > 0 ? 2 : 1
        Accessible.name: "Effect " + root.effectNumber + ": " + root.slot.effectName
                + (controllerSelected.value > 0 ? ", selected on controller" : "")
    }

    Effects.EffectSelector {
        id: selector
        anchors.left: enableButton.right
        anchors.leftMargin: 4
        anchors.right: beatReadout.left
        anchors.rightMargin: 3
        anchors.verticalCenter: parent.verticalCenter
        height: 32
        slot: root.slot
        font.pixelSize: 11

        background: Rectangle {
            color: selector.down ? "#14191e" : "#242a30"
            border.color: selector.activeFocus ? root.accent : "#39434c"
            border.width: 1
            radius: 3

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: 1
                color: "#48515a"
                opacity: selector.down ? 0.15 : 0.7
            }
        }
        contentItem: Text {
            leftPadding: 8
            rightPadding: 21
            text: selector.displayText
            color: "#e3e8ed"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Item {
            x: selector.width - width - 8
            anchors.verticalCenter: parent.verticalCenter
            width: 9
            height: 8

            Canvas {
                anchors.fill: parent
                onPaint: {
                    const painter = getContext("2d");
                    painter.clearRect(0, 0, width, height);
                    painter.fillStyle = "#9ba8b2";
                    painter.beginPath();
                    painter.moveTo(0, 2);
                    painter.lineTo(width, 2);
                    painter.lineTo(width / 2, height - 1);
                    painter.closePath();
                    painter.fill();
                }
            }
        }
        delegate: ItemDelegate {
            id: option
            required property int index
            width: selector.popup.width
            height: 40
            text: selector.textAt(index)
            highlighted: selector.highlightedIndex === index

            background: Rectangle {
                color: option.highlighted ? "#36434d" : "#1c2329"
            }
            contentItem: Text {
                text: option.text
                color: "#e3e8ed"
                font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                elide: Text.ElideRight
            }
        }
        popup: Popup {
            y: selector.height - 1
            width: Math.max(selector.width, 190)
            height: Math.min(240, selector.count * 40 + 2)
            padding: 1

            background: Rectangle {
                color: "#1c2329"
                border.color: "#596771"
                radius: 4
            }
            contentItem: ListView {
                clip: true
                model: selector.popup.visible ? selector.delegateModel : null
                currentIndex: selector.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
        }
    }
}
