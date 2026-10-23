// A paragraph whose brewing terms (OG, racking, sorbate…) are underlined; tap one to see it in everyday words.
// Use it like a Label: set text, color and font. The wording comes from the word list in the tested engine.
import QtQuick
import QtQuick.Controls
import MeadWorkshop.Core

Item {
    id: root
    property string text
    property color color: Theme.ink2
    property alias font: lab.font
    property alias wrapMode: lab.wrapMode
    implicitWidth: lab.implicitWidth
    implicitHeight: lab.implicitHeight

    Label {
        id: lab
        width: root.width
        text: Designer.linked(root.text)
        textFormat: Text.StyledText
        color: root.color
        linkColor: Theme.accent
        wrapMode: Text.WordWrap
        onLinkActivated: function(link) { Designer.explainTerm(link) }
        HoverHandler { cursorShape: lab.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }
}
