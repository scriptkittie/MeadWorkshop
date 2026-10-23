// A drop-down over [{id, name}] that follows `current` and reports the user's pick.
import QtQuick
import MeadWorkshop.Core
import QtQuick.Controls

ComboBox {
    id: c
    property var current
    signal picked(var value)
    textRole: "name"
    valueRole: "id"
    function sync() {
        const i = indexOfValue(current)
        if (i !== currentIndex) currentIndex = i
    }
    onCurrentChanged: sync()
    onModelChanged: sync()
    Component.onCompleted: sync()
    onActivated: picked(currentValue)
}
