// Directions for the recipe in the Designer: what to buy, what to do before and on brew day, and a dated timeline after.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeadWorkshop.Core

ScrollView {
    id: scroller
    contentWidth: availableWidth
    readonly property var d: Designer.design
    readonly property var dir: Designer.results.directions

    component Body: Prose { color: Theme.ink2; Layout.fillWidth: true; font.pixelSize: 14 }   // underlined terms open the word list
    component Src: Label { color: Theme.ink3; font.pixelSize: 11; font.italic: true }
    component Steps: ColumnLayout {
        id: stepsBox
        property var model: []
        property int startAt: 1
        Layout.fillWidth: true
        spacing: 12
        Repeater {
            model: stepsBox.model
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    width: 28; height: 28; radius: 14
                    color: Theme.alpha(Theme.accent, 0.18)
                    border.color: Theme.accent
                    Label { anchors.centerIn: parent; text: stepsBox.startAt + index; color: Theme.accent; font.bold: true }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: modelData.title; color: Theme.ink; font.bold: true; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Body { text: modelData.detail }
                    Src { text: modelData.src; visible: text !== "" }
                }
            }
        }
    }

    ColumnLayout {
        width: Math.min(scroller.availableWidth - 32, 980)
        x: Math.max(16, (scroller.availableWidth - width) / 2)
        spacing: 14

        Item { Layout.preferredHeight: 4 }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: qsTr("Directions: ") + d.name; font.pixelSize: 20; font.bold: true; color: Theme.ink; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label {
                    text: qsTr("For the recipe in the Designer · %1 gal · brew day %2").arg(Number(Designer.results.finalVolume).toFixed(2)).arg(d.brewDate)
                    color: Theme.ink3
                }
                Label {
                    visible: dir && dir.brewDateValid
                    text: qsTr("The dates below count from the brew date. Brewing on a different day? Change Brew date in the Designer and every date moves with it.")
                    color: Theme.ink3
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        Label {
            visible: dir && !dir.brewDateValid
            text: qsTr("Set a brew date (YYYY-MM-DD) in the Designer to see calendar dates.")
            color: Theme.warn
        }

        Card {
            title: qsTr("What you need")
            Repeater {
                model: dir ? dir.shopping : []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: modelData.what; color: Theme.ink; Layout.preferredWidth: 260; wrapMode: Text.WordWrap; font.pixelSize: 14 }
                    Label { text: modelData.amount; color: Theme.accent; font.bold: true; Layout.preferredWidth: 160; Layout.maximumWidth: 160; wrapMode: Text.WordWrap; font.pixelSize: 14 }
                    Label { text: modelData.note; color: Theme.ink3; Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 13 }
                }
            }
        }

        Card {
            visible: dir && dir.prep.length > 0
            title: qsTr("Before brew day")
            Steps { model: dir ? dir.prep : [] }
        }

        Card {
            visible: dir && dir.brewDay.length > 0
            title: qsTr("Brew day")
            Steps { model: dir ? dir.brewDay : []; startAt: dir ? dir.prep.length + 1 : 1 }
        }

        Card {
            title: d.wild ? qsTr("Wild fermentation") : qsTr("After brew day")
            Repeater {
                model: dir ? dir.timeline : []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 150
                        text: modelData.when
                        color: Theme.accent
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: modelData.title; color: Theme.ink; font.bold: true; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Body { text: modelData.detail }
                        Src { text: modelData.src; visible: text !== "" }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.ink3
            font.pixelSize: 12
            text: qsTr("Steps follow Piatz's basic process (ch.5) with your recipe's amounts; timings come from Piatz and Schramm as cited. Change the recipe in the Designer and these update.")
        }
        Item { Layout.preferredHeight: 16 }
    }
}
