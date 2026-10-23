// A horizontal bar with shaded zones, a predicted-value line, an optional target diamond and range band.
import QtQuick
import MeadWorkshop.Core
import QtQuick.Controls

Item {
    id: meter
    property real minV: 0
    property real maxV: 1
    property var zones: []          // [{from, to, label, shade}]
    property real value: 0
    property var target: undefined  // number, or undefined for none
    property var band: undefined    // [lo, hi], or undefined
    implicitHeight: 28

    function px(v) { return Math.max(0, Math.min(width, (v - minV) / (maxV - minV) * width)) }

    Rectangle { anchors.fill: parent; radius: 6; color: Theme.panel2; border.color: Theme.line }
    Repeater {
        model: meter.zones
        delegate: Rectangle {
            x: meter.px(modelData.from)
            width: Math.max(0, meter.px(modelData.to) - meter.px(modelData.from))
            y: 1
            height: meter.height - 2
            color: Theme.zone(modelData.shade)
            clip: true
            Label {
                anchors.centerIn: parent
                text: modelData.label
                color: Theme.ink2
                font.pixelSize: 11
                visible: parent.width > implicitWidth + 4
            }
        }
    }
    Rectangle {
        visible: meter.band !== undefined
        x: meter.band !== undefined ? meter.px(meter.band[0]) : 0
        width: meter.band !== undefined ? Math.max(3, meter.px(meter.band[1]) - meter.px(meter.band[0])) : 0
        y: meter.height / 2 - 3
        height: 6
        radius: 3
        color: Theme.alpha(Theme.ink, 0.25)
    }
    Rectangle {   // target ◆
        visible: meter.target !== undefined
        width: 12; height: 12
        rotation: 45
        x: (meter.target !== undefined ? meter.px(meter.target) : 0) - 6
        y: -7
        color: Theme.accent
        border.color: Theme.panel
    }
    Rectangle {   // predicted ▮
        x: meter.px(meter.value) - 2
        y: -3
        width: 4
        height: meter.height + 6
        radius: 2
        color: Theme.ink
        border.color: Theme.panel
    }
}
