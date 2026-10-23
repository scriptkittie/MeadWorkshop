// A rounded panel with an optional title row.
import QtQuick
import MeadWorkshop.Core
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card
    property string title: ""
    default property alias content: body.data
    property alias headerExtras: extras.data

    Layout.fillWidth: true
    radius: 10
    color: Theme.panel
    border.color: Theme.line
    implicitHeight: col.implicitHeight + 28

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10
        RowLayout {
            visible: card.title !== "" || extras.children.length > 0
            Layout.fillWidth: true
            Label { text: card.title; font.pixelSize: 16; font.bold: true; color: Theme.ink; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout { id: extras; spacing: 6 }
        }
        ColumnLayout { id: body; Layout.fillWidth: true; spacing: 8 }
    }
}
