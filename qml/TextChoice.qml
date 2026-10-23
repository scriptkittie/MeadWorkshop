// Free text with suggestions (honey varieties, sugars, adjuncts). Commits on Enter, pick, or leaving the box.
import QtQuick
import MeadWorkshop.Core
import QtQuick.Controls

ComboBox {
    id: c
    property string current
    signal committed(string text)
    editable: true
    function sync() { if (editText !== current) editText = current }
    onCurrentChanged: sync()
    Component.onCompleted: sync()
    onAccepted: if (editText !== current) committed(editText)
    onActivated: function(i) { const t = textAt(i); editText = t; if (t !== current) committed(t) }
    onActiveFocusChanged: if (!activeFocus && editText !== current) committed(editText)
}
