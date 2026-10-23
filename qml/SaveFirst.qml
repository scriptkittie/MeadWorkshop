// Asks "save first?" before something replaces the recipe on screen (New, Open, a book recipe, a plan).
// Use: saveFirst.ask(function() { Designer.newDesign() })  (runs right away when nothing is unsaved)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeadWorkshop.Core

Dialog {
    id: root
    property var proceed: null
    function ask(action) {
        if (!Designer.unsaved) { action(); return }
        proceed = action
        open()
    }
    function go() {
        const a = proceed
        proceed = null
        close()
        if (a) a()
    }
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 40 : 460, 460)
    modal: true
    title: qsTr("Save your recipe first?")
    background: Rectangle { color: Theme.panel; border.color: Theme.line; radius: 10 }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.ink2
            text: Designer.savedAs !== ""
                  ? qsTr("“%1” has changes that aren't saved yet.").arg(Designer.savedAs)
                  : qsTr("“%1” isn't saved in My recipes yet.").arg(Designer.design.name || qsTr("This recipe"))
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Cancel"); onClicked: { root.proceed = null; root.close() } }
            Button { text: qsTr("Don't save"); onClicked: root.go() }
            Button {
                text: qsTr("Save")
                highlighted: true
                onClicked: { Designer.saveRecipe(); if (!Designer.unsaved) root.go() }
            }
        }
    }
}
