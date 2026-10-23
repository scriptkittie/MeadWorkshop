// The word list: every brewing term in everyday words, with its real name and where the books cover it.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeadWorkshop.Core

ScrollView {
    id: scroller
    contentWidth: availableWidth
    readonly property var entries: Designer.glossary()
    readonly property var groupNames: Designer.glossaryGroups()
    property string filter: ""
    property string openKey: ""

    function shows(e) {
        const f = scroller.filter.trim().toLowerCase()
        return f === "" || e.term.toLowerCase().indexOf(f) >= 0 || e.plain.toLowerCase().indexOf(f) >= 0 || e.what.toLowerCase().indexOf(f) >= 0
    }
    // Opened from a tapped word elsewhere: show that entry.
    function reveal(key) { scroller.filter = ""; search.text = ""; scroller.openKey = key }

    ColumnLayout {
        width: scroller.availableWidth
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            spacing: 6
            Label { text: qsTr("Words"); font.pixelSize: 20; font.bold: true; color: Theme.ink }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink3
                font.pixelSize: 13
                text: qsTr("Every brewing term the app uses, in everyday words first with the real name beside it. Tap a row to read more. Elsewhere in the app, underlined words open the same explanation.")
            }
            TextField {
                id: search
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                placeholderText: qsTr("Search, e.g. OG, racking, sweet…")
                onTextChanged: scroller.filter = text
            }
        }

        Repeater {
            model: scroller.groupNames
            delegate: Card {
                id: groupCard
                required property string modelData
                readonly property string groupName: modelData
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                title: groupName
                visible: scroller.entries.some(e => e.group === groupName && scroller.shows(e))
                Repeater {
                    model: scroller.entries.filter(e => e.group === groupCard.groupName && scroller.shows(e))
                    delegate: ColumnLayout {
                        id: row
                        required property var modelData
                        readonly property bool open: scroller.openKey === modelData.key || scroller.filter.trim() !== ""
                        Layout.fillWidth: true
                        spacing: 3
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: row.open ? "▾" : "▸"; color: Theme.accent; font.pixelSize: 13 }
                            Label { text: row.modelData.plain; color: Theme.ink; font.bold: true; font.pixelSize: 14 }
                            Label { text: row.modelData.term; color: Theme.ink3; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
                            TapHandler { onTapped: scroller.openKey = row.open && scroller.openKey === row.modelData.key ? "" : row.modelData.key }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }
                        Prose {
                            visible: row.open
                            Layout.fillWidth: true
                            Layout.leftMargin: 22
                            font.pixelSize: 13
                            text: row.modelData.what
                        }
                        Label {
                            visible: row.open && row.modelData.src !== ""
                            Layout.leftMargin: 22
                            text: row.modelData.src
                            color: Theme.ink3
                            font.pixelSize: 11
                            font.italic: true
                        }
                    }
                }
            }
        }
        Item { Layout.preferredHeight: 16 }
    }
}
