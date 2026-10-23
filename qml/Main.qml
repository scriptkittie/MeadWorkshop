// Mead Workshop — main window: tabs for each screen, plus settings.
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeadWorkshop.Core

ApplicationWindow {
    id: win
    width: 1280
    height: 820
    minimumWidth: 700
    minimumHeight: 500
    visible: true
    title: qsTr("Mead Workshop")

    // A tab with an amber underline when chosen.
    component Tab: TabButton {
        id: tb
        width: implicitWidth + 20
        contentItem: Label { text: tb.text; color: tb.checked ? Theme.ink : Theme.ink3; font.bold: tb.checked; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle {
            color: tb.hovered && !tb.checked ? Theme.panel2 : "transparent"
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 3; radius: 1; color: tb.checked ? Theme.accent : "transparent" }
        }
    }

    Settings {
        id: prefs
        property alias dark: win.darkPref
        property alias tab: tabs.currentIndex
    }
    property bool darkPref: true
    Binding { target: Theme; property: "dark"; value: win.darkPref }

    color: Theme.bg
    palette.window: Theme.bg
    palette.base: Theme.panel
    palette.alternateBase: Theme.panel2
    palette.text: Theme.ink
    palette.windowText: Theme.ink
    palette.buttonText: Theme.ink
    palette.button: Theme.panel2
    palette.light: Theme.panel2
    palette.midlight: Theme.line
    palette.mid: Theme.line
    palette.dark: Theme.ink3
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.onAccent
    palette.placeholderText: Theme.ink3
    palette.toolTipBase: Theme.panel2
    palette.toolTipText: Theme.ink

    header: ToolBar {
        background: Rectangle {
            color: Theme.bg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 10
            Label { text: "⬢"; color: Theme.accent; font.pixelSize: 22 }
            Label { text: qsTr("Mead Workshop"); font.pixelSize: 19; font.bold: true; color: Theme.ink }
            TabBar {
                id: tabs
                Layout.leftMargin: 16
                background: Item {}
                Tab { text: qsTr("Plan") }
                Tab { text: qsTr("Designer") }
                Tab { text: qsTr("Directions") }
                Tab { text: qsTr("Quick check") }
                Tab { text: qsTr("Words") }
            }
            Item { Layout.fillWidth: true }
            Button { text: qsTr("⚙  Settings"); onClicked: settingsPopup.open() }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex
        PlanPage {}
        DesignerPage {}
        DirectionsPage {}
        QuickCheckPage {}
        WordsPage { id: wordsPage }
    }
    // "Open in Designer" on the Plan tab switches to the Designer.
    Connections {
        target: Designer
        function onOpenDesigner() { tabs.currentIndex = 1 }
        function onTermRequested(key) { termPopup.entry = Designer.term(key); termPopup.open() }
    }

    // A tapped term, explained in everyday words.
    Popup {
        id: termPopup
        property var entry: ({})
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, win.width - 40)
        padding: 18
        modal: true
        dim: true
        background: Rectangle { color: Theme.panel; border.color: Theme.line; radius: 10 }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: termPopup.entry.plain || ""; font.pixelSize: 17; font.bold: true; color: Theme.ink; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Label { text: termPopup.entry.term || ""; color: Theme.accent; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Prose { text: termPopup.entry.what || ""; Layout.fillWidth: true; font.pixelSize: 14 }
            Label { visible: text !== ""; text: termPopup.entry.src || ""; color: Theme.ink3; font.pixelSize: 11; font.italic: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: qsTr("All words"); onClicked: { wordsPage.reveal(termPopup.entry.key); tabs.currentIndex = 4; termPopup.close() } }
                Button { text: qsTr("Close"); highlighted: true; onClicked: termPopup.close() }
            }
        }
    }

    Popup {
        id: settingsPopup
        x: win.width - width - 12
        y: 4
        width: 380
        padding: 16
        modal: true
        background: Rectangle { color: Theme.panel; border.color: Theme.line; radius: 10 }
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Label { text: qsTr("Settings"); font.pixelSize: 16; font.bold: true; color: Theme.ink }

            Label { text: qsTr("Appearance"); color: Theme.ink3; font.pixelSize: 12 }
            RowLayout {
                RadioButton { text: qsTr("Dark"); checked: win.darkPref; onToggled: if (checked) win.darkPref = true }
                RadioButton { text: qsTr("Light"); checked: !win.darkPref; onToggled: if (checked) win.darkPref = false }
            }

            Label { text: qsTr("Sweetness scale"); color: Theme.ink3; font.pixelSize: 12 }
            RowLayout {
                RadioButton { text: qsTr("Piatz (BJCP)"); checked: Designer.sweetnessScale === "piatz"; onToggled: if (checked) Designer.sweetnessScale = "piatz" }
                RadioButton { text: qsTr("Schramm"); checked: Designer.sweetnessScale === "schramm"; onToggled: if (checked) Designer.sweetnessScale = "schramm" }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink3
                font.pixelSize: 12
                text: qsTr("Piatz: dry 0.990–1.010, semisweet 1.010–1.025, sweet 1.025–1.050. Schramm: dry 0.990–1.006, medium 1.006–1.015, sweet 1.012–1.020, dessert 1.020+.")
            }

            Label { text: qsTr("Honey gravity (points per lb per gallon)"); color: Theme.ink3; font.pixelSize: 12 }
            RowLayout {
                RadioButton { text: "38"; checked: Designer.honeyPPG === 38; onToggled: if (checked) Designer.honeyPPG = 38 }
                RadioButton { text: qsTr("40 (books)"); checked: Designer.honeyPPG === 40; onToggled: if (checked) Designer.honeyPPG = 40 }
                RadioButton { text: "42"; checked: Designer.honeyPPG === 42; onToggled: if (checked) Designer.honeyPPG = 42 }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink3
                font.pixelSize: 12
                text: qsTr("40 is the books' rule (Piatz's chart, Schramm). Piatz's printed recipes imply about 42. Chemistry (USDA honey, 82.9% solids) gives about 38.")
            }
            Label { text: qsTr("About"); color: Theme.ink3; font.pixelSize: 12 }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink2
                font.pixelSize: 12
                text: qsTr("Mead Workshop %1. Free and open source (MIT license). Methods from Piatz, Schramm and Katz; every figure names its source.").arg(Qt.application.version)
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Open My recipes folder"); onClicked: Designer.showRecipesFolder() }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Close"); onClicked: settingsPopup.close() }
            }
        }
    }
}
