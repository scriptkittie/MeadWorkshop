// Quick check: honey + yeast → OG, FG, ABV, with the sweetness and strength bars.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeadWorkshop.Core

ScrollView {
    id: scroller
    contentWidth: availableWidth

    property real gallons: Number(galField.text.replace(",", ".")) > 0 ? Number(galField.text.replace(",", ".")) : 0
    property real honeyLb: Number(honeyField.text.replace(",", ".")) > 0 ? Number(honeyField.text.replace(",", ".")) : 0
    readonly property var yeastList: Mead.yeasts()
    property real tolerance: yeastBox.currentIndex >= 0 && yeastList.length > 0 ? yeastList[yeastBox.currentIndex].tolerance : 14
    readonly property var res: Mead.quickCheck(gallons, honeyLb, tolerance, Designer.sweetnessScale)

    ColumnLayout {
        width: scroller.availableWidth - 32
        x: 16
        spacing: 14
        Item { Layout.preferredHeight: 4 }

        Card {
            title: qsTr("Quick check: a traditional mead")
            GridLayout {
                columns: 3
                columnSpacing: 12
                rowSpacing: 4
                Layout.fillWidth: true
                Label { text: qsTr("Batch (gallons)"); color: Theme.ink2 }
                Label { text: qsTr("Honey (lb)"); color: Theme.ink2 }
                Label { text: qsTr("Yeast"); color: Theme.ink2 }
                TextField { id: galField; text: "5"; inputMethodHints: Qt.ImhFormattedNumbersOnly; Layout.preferredWidth: 140; selectByMouse: true }
                TextField { id: honeyField; text: "14"; inputMethodHints: Qt.ImhFormattedNumbersOnly; Layout.preferredWidth: 140; selectByMouse: true }
                ComboBox {
                    id: yeastBox
                    Layout.fillWidth: true
                    model: scroller.yeastList
                    textRole: "name"
                    Component.onCompleted: {
                        for (let i = 0; i < scroller.yeastList.length; ++i)
                            if (scroller.yeastList[i].name.indexOf("71B") >= 0) { currentIndex = i; break }
                    }
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 24
                Repeater {
                    model: [
                        { k: qsTr("Starting sugar (OG)"), v: scroller.res.og.toFixed(3) },
                        { k: qsTr("Ending sugar (FG)"), v: scroller.res.fg.toFixed(3) },
                        { k: qsTr("Alcohol (ABV)"), v: scroller.res.abv.toFixed(1) + "%" },
                        { k: qsTr("Water to pour in"), v: scroller.res.waterToAdd.toFixed(2) + " gal" }
                    ]
                    delegate: Column {
                        spacing: 0
                        Label { text: modelData.k.toUpperCase(); color: Theme.ink3; font.pixelSize: 11 }
                        Label { text: modelData.v; color: Theme.accent; font.pixelSize: 24 }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Sweetness — ") + scroller.res.sweetness + (scroller.res.reachesDry ? qsTr(" (the yeast should ferment it dry)") : qsTr(" (the yeast stops before all the sugar is gone)"))
                color: Theme.ink
            }
            Meter {
                Layout.fillWidth: true
                minV: 0.990
                maxV: 1.050
                value: scroller.res.fg
                zones: {
                    const L = Mead.sweetnessLevels(Designer.sweetnessScale)
                    const z = []
                    for (let i = 0; i < L.length; ++i)
                        z.push({ from: Math.max(0.990, L[i].lo), to: i < L.length - 1 ? L[i + 1].lo : 1.050, label: L[i].label, shade: i })
                    return z
                }
            }
            Label { text: qsTr("Strength — ") + scroller.res.strength; color: Theme.ink }
            Meter {
                Layout.fillWidth: true
                minV: 0
                maxV: 20
                value: scroller.res.abv
                target: scroller.tolerance
                zones: [{ from: 0, to: 3.5, label: "", shade: 0 }, { from: 3.5, to: 7.5, label: qsTr("Hydromel"), shade: 1 },
                        { from: 7.5, to: 14, label: qsTr("Standard"), shade: 2 }, { from: 14, to: 18, label: qsTr("Sack"), shade: 3 },
                        { from: 18, to: 20, label: "", shade: 4 }]
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink3
                font.pixelSize: 12
                text: qsTr("Uses the books' rules: 40 gravity points per lb of honey per gallon, Piatz's yeast table, and ABV = 131 × (OG − FG). Try 14 lb in 5 gal with 71B: Piatz's Orange Blossom.")
            }
        }

        Card {
            title: qsTr("Self-check")
            Label { text: "✓ " + Mead.dataSummary(); color: Theme.ink2 }
            Label { text: "✓ " + Mead.selfCheck(); color: Theme.ink2; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
        Item { Layout.preferredHeight: 12 }
    }
}
