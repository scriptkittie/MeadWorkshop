// A number box that commits when you press Enter or leave it. Blank is allowed (means "use the default").
import QtQuick
import MeadWorkshop.Core
import QtQuick.Controls
import QtQuick.Layouts

TextField {
    id: f
    property var value
    signal committed(string text)
    text: Theme.plain(value)
    selectByMouse: true
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    Layout.preferredWidth: 90
    onEditingFinished: if (text !== Theme.plain(value)) committed(text)
}
