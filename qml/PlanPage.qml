// Plan a mead from a goal: say what you want and what you have; the app works out the rest and explains why.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeadWorkshop.Core

Item {
    id: page
    readonly property var g: Designer.goal
    readonly property var p: Designer.plan
    readonly property bool wide: width >= 1050
    readonly property var fruitList: Designer.fruitOptions()
    readonly property var yeastList: Designer.yeastChoices   // marked for your goal: ★ suggested · ✓ good · * catch · ✗ not suitable
    readonly property var strategies: [{ id: "auto", name: qsTr("Choose for me") },
                                       { id: "tolerance", name: qsTr("Yeast stops sweet at its limit") },
                                       { id: "dry", name: qsTr("Ferment dry, then backsweeten") }]
    readonly property var characters: [{ id: "all", name: qsTr("Use all I have") }, { id: "mild", name: qsTr("Mild") },
                                       { id: "medium", name: qsTr("Medium") }, { id: "strong", name: qsTr("Strong") }]
    readonly property var whens: [{ id: "auto", name: qsTr("Recommend") }, { id: "primary", name: qsTr("Brew day") },
                                  { id: "secondary", name: qsTr("Later (~day 21)") }]
    readonly property var timings: [{ id: "piatz", name: qsTr("Brew day (Piatz)") }, { id: "schramm", name: qsTr("Later, after 2–4 weeks (Schramm)") }]
    readonly property var spiceList: Designer.spiceOptions()
    readonly property var spiceLevels: [{ id: "subtle", name: qsTr("Subtle") }, { id: "noticeable", name: qsTr("Noticeable") }, { id: "strong", name: qsTr("Strong") }]
    readonly property var fills: [{ id: "two-thirds", name: qsTr("Two-thirds (Piatz, room for foam)") },
                                  { id: "three-quarters", name: qsTr("Three-quarters (more mead, stir gently)") }]

    component FieldLabel: Label { color: Theme.ink3; font.pixelSize: 12 }
    component Muted: Prose { color: Theme.ink3; font.pixelSize: 12; Layout.fillWidth: true }   // underlined terms open the word list
    component Body: Prose { color: Theme.ink2; Layout.fillWidth: true; font.pixelSize: 13 }
    component Src: Label { color: Theme.ink3; font.pixelSize: 11; font.italic: true }
    component Stat: Rectangle {
        id: stat
        property string k
        property string v
        property string s
        property bool hl: false
        property string term: ""   // word-list key: tap the heading to explain it
        Layout.fillWidth: true
        Layout.preferredWidth: 150
        implicitHeight: statCol.implicitHeight + 16
        radius: 8
        color: stat.hl ? Theme.alpha(Theme.accent, 0.14) : Theme.panel2
        border.color: stat.hl ? Theme.accent : Theme.line
        ColumnLayout {
            id: statCol
            anchors.fill: parent
            anchors.margins: 8
            spacing: 1
            Label {
                text: stat.k.toUpperCase() + (stat.term !== "" ? "  ⓘ" : "")
                color: Theme.ink3
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                TapHandler { enabled: stat.term !== ""; onTapped: Designer.explainTerm(stat.term) }
                HoverHandler { enabled: stat.term !== ""; cursorShape: Qt.PointingHandCursor }
            }
            Label { text: stat.v; color: Theme.accent; font.pixelSize: 22 }
            Label { text: stat.s; color: Theme.ink3; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true; visible: text !== "" }
        }
    }
    component Row2: RowLayout {
        property string k
        property string v
        property string s
        Layout.fillWidth: true
        spacing: 10
        Label { text: parent.k; color: Theme.ink; Layout.preferredWidth: 190; wrapMode: Text.WordWrap; font.pixelSize: 13 }
        Label { text: parent.v; color: Theme.accent; font.bold: true; Layout.preferredWidth: 90; font.pixelSize: 13 }
        Prose { text: parent.s; color: Theme.ink3; Layout.fillWidth: true; font.pixelSize: 12 }
    }

    ScrollView {
        id: scroller
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: scroller.availableWidth - 32
            x: 16
            spacing: 14

            Item { Layout.preferredHeight: 4 }
            Label { text: qsTr("Plan a mead from a goal"); font.pixelSize: 20; font.bold: true; color: Theme.ink }
            Muted {
                font.pixelSize: 13
                text: qsTr("Say what you want and what you have. The planner sizes the fruit from Schramm's table (or uses all you have), picks a yeast if you don't, chooses how to reach your sweetness, solves the honey with the fruit's water and sugar counted (Piatz's method), and explains each step with its source.")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: page.wide ? 2 : 1
                columnSpacing: 16
                rowSpacing: 14

                // ================================================= the goal
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    spacing: 14

                    Card {
                        title: qsTr("What you want")
                        GridLayout {
                            columns: 3
                            columnSpacing: 10
                            rowSpacing: 4
                            Layout.fillWidth: true
                            FieldLabel { text: qsTr("Name") }
                            FieldLabel { text: qsTr("Batch (gal)") }
                            FieldLabel { text: qsTr("Target ABV %") }
                            TextField {
                                Layout.fillWidth: true
                                text: g.name
                                selectByMouse: true
                                onEditingFinished: if (text !== g.name) Designer.setGoalField("name", text)
                            }
                            NumField { value: g.volumeGal; onCommitted: function(t) { Designer.setGoalField("volumeGal", t) } }
                            NumField { value: g.abv; onCommitted: function(t) { Designer.setGoalField("abv", t) } }
                        }
                        GridLayout {
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 4
                            Layout.fillWidth: true
                            FieldLabel { text: qsTr("Honey") }
                            FieldLabel { text: qsTr("Sweetness") }
                            TextChoice {
                                Layout.fillWidth: true
                                model: Designer.honeyNames()
                                current: g.honeyName
                                onCommitted: function(t) { Designer.setGoalField("honeyName", t) }
                            }
                            Choice {
                                Layout.fillWidth: true
                                model: Designer.sweetnessChoices()
                                textRole: "label"
                                valueRole: "key"
                                current: g.sweetness
                                onPicked: function(v) { Designer.setGoalField("sweetness", v) }
                            }
                            FieldLabel { text: qsTr("Yeast") }
                            FieldLabel { text: qsTr("How to reach the sweetness") }
                            Choice {
                                Layout.fillWidth: true
                                model: page.yeastList
                                current: g.yeastId
                                onPicked: function(v) { Designer.setGoalField("yeastId", v) }
                            }
                            Choice {
                                Layout.fillWidth: true
                                model: page.strategies
                                current: g.strategy
                                onPicked: function(v) { Designer.setGoalField("strategy", v) }
                            }
                            FieldLabel { text: qsTr("Your fermenter (gal)") }
                            FieldLabel { text: qsTr("Fruit timing (when left on Recommend)") }
                            NumField {
                                value: Designer.fermenterGal
                                placeholderText: qsTr("no limit")
                                onCommitted: function(t) { const n = Number(t.replace(",", ".")); Designer.fermenterGal = t === "" || isNaN(n) ? 0 : n }
                            }
                            Choice {
                                Layout.fillWidth: true
                                model: page.timings
                                current: g.fruitTiming
                                onPicked: function(v) { Designer.setGoalField("fruitTiming", v) }
                            }
                            FieldLabel { text: qsTr("Fill the fermenter on brew day up to") }
                            Item { implicitHeight: 1 }
                            Choice {
                                Layout.fillWidth: true
                                model: page.fills
                                current: g.fill || "two-thirds"
                                onPicked: function(v) { Designer.setGoalField("fill", v) }
                            }
                            Item { implicitHeight: 1 }
                        }
                        Muted { text: qsTr("The fermenter size keeps brew day under two-thirds full, Piatz's rule for leaving room for foam. Acidic fruit like lemons and limes always goes in later.") }
                    }

                    Card {
                        title: qsTr("What you have")
                        headerExtras: [Button { text: qsTr("+ Fruit"); onClicked: Designer.addGoalFruit() }]
                        Repeater {
                            model: g.fruits.length
                            delegate: GridLayout {
                                readonly property var row: g.fruits[index]
                                columns: 5
                                columnSpacing: 8
                                rowSpacing: 2
                                Layout.fillWidth: true
                                FieldLabel { text: qsTr("Fruit") }
                                FieldLabel { text: qsTr("On hand (lb)") }
                                FieldLabel { text: qsTr("Character") }
                                FieldLabel { text: qsTr("When") }
                                Item { width: 1 }
                                Choice {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 140
                                    model: page.fruitList
                                    current: row ? row.fruitId : ""
                                    onPicked: function(v) { Designer.setGoalField("fruits." + index + ".fruitId", v) }
                                }
                                NumField {
                                    Layout.preferredWidth: 80
                                    value: row ? row.haveLb : ""
                                    placeholderText: qsTr("any")
                                    onCommitted: function(t) { Designer.setGoalField("fruits." + index + ".haveLb", t) }
                                }
                                Choice {
                                    Layout.preferredWidth: 150
                                    model: page.characters
                                    current: row ? row.intensity : "medium"
                                    onPicked: function(v) { Designer.setGoalField("fruits." + index + ".intensity", v) }
                                }
                                Choice {
                                    Layout.preferredWidth: 130
                                    model: page.whens
                                    current: row ? row.phase : "auto"
                                    onPicked: function(v) { Designer.setGoalField("fruits." + index + ".phase", v) }
                                }
                                ToolButton { text: "×"; font.pixelSize: 18; onClicked: Designer.removeGoalFruit(index) }
                            }
                        }
                        Muted { visible: g.fruits.length === 0; text: qsTr("No fruit makes a traditional mead.") }
                        Muted { text: qsTr("\"Use all I have\" puts every pound in and solves the honey around it. Mild / Medium / Strong size the fruit from Schramm's Table 8.1 instead (or Piatz's 1–2 / 4–5 lb per gallon guide).") }
                    }

                    Card {
                        title: qsTr("Spices, herbs & oak")
                        headerExtras: [Button { text: qsTr("+ Spice"); onClicked: Designer.addGoalSpice() }]
                        Repeater {
                            model: (g.spices || []).length
                            delegate: RowLayout {
                                readonly property var row: g.spices[index]
                                Layout.fillWidth: true
                                spacing: 8
                                Choice {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 180
                                    model: page.spiceList
                                    current: row ? row.spiceId : ""
                                    onPicked: function(v) { Designer.setGoalField("spices." + index + ".spiceId", v) }
                                }
                                Choice {
                                    Layout.preferredWidth: 150
                                    model: page.spiceLevels
                                    current: row ? row.level : "noticeable"
                                    onPicked: function(v) { Designer.setGoalField("spices." + index + ".level", v) }
                                }
                                ToolButton { text: "×"; font.pixelSize: 18; onClicked: Designer.removeGoalSpice(index) }
                            }
                        }
                        Muted { text: qsTr("Amounts come from Piatz and Schramm where they give one, and from homebrewers' forums where they don't (the plan says which). Each is a starting point: taste as it goes and work up to your liking.") }
                    }

                    Button {
                        text: qsTr("Plan it")
                        highlighted: true
                        Layout.alignment: Qt.AlignRight
                        onClicked: Designer.runPlan()
                    }
                }

                // ================================================= the plan
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    spacing: 14

                    Card {
                        visible: !p.ready
                        Body { text: qsTr("Fill in your goal and press \"Plan it\". The recipe and the reasoning behind it will appear here.") }
                    }

                    Card {
                        visible: p.ready === true
                        title: qsTr("In short")
                        Repeater {
                            model: p.summary || []
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Label { text: (index + 1) + "."; color: Theme.accent; font.bold: true; Layout.alignment: Qt.AlignTop; font.pixelSize: 15 }
                                Prose { text: modelData; color: Theme.ink; Layout.fillWidth: true; font.pixelSize: 15 }
                            }
                        }
                        Muted { visible: p.reachable !== false; text: qsTr("Press \"Open in Designer\", then the Directions tab, for every step with dates.") }
                    }

                    Card {
                        visible: p.ready === true && p.reachable !== false
                        title: p.name || ""
                        headerExtras: [Button { text: qsTr("Open in Designer"); highlighted: true; onClicked: saveFirst.ask(function() { Designer.usePlan() }) }]
                        GridLayout {
                            columns: 3
                            columnSpacing: 8
                            rowSpacing: 8
                            Layout.fillWidth: true
                            Stat { k: qsTr("Starting sugar (OG)"); term: "effOG"; v: Theme.sg3(p.effectiveOG); s: qsTr("with all the fruit · target ") + Theme.sg3(p.targetOG); hl: true }
                            Stat { k: qsTr("Ending sugar (FG)"); term: "fg"; v: Theme.sg3(p.fg); s: p.sweetnessLabel || "" }
                            Stat { k: qsTr("Alcohol (ABV)"); term: "abv"; v: p.abv !== undefined ? Number(p.abv).toFixed(1) + "%" : ""; s: p.strength || "" }
                        }
                        Body { text: qsTr("Strategy: ") + (p.strategy || ""); color: Theme.ink }
                        Body {
                            visible: p.style !== undefined
                            color: Theme.ink
                            text: p.style ? qsTr("Style: ") + p.style.name + qsTr(" (") + p.style.because + qsTr(")") : ""
                        }
                        Muted { visible: p.style !== undefined; text: p.style ? p.style.plain + qsTr("  In words: ") + p.style.words + "." : "" }

                        Label { text: qsTr("Recipe"); font.bold: true; color: Theme.ink; Layout.topMargin: 4 }
                        Repeater {
                            model: p.honeys || []
                            delegate: Row2 { k: modelData.name; v: modelData.amount; s: qsTr("stir into water first") }
                        }
                        Row2 { k: qsTr("Water to pour in"); v: p.waterToAdd || ""; s: p.waterHint || "" }
                        Repeater {
                            model: p.fruits || []
                            delegate: Row2 { k: modelData.name; v: modelData.amount; s: modelData.when }
                        }
                        Repeater {
                            model: p.extras || []
                            delegate: Row2 { k: modelData.name; v: ""; s: modelData.when }
                        }
                        Row2 { k: qsTr("Final volume"); v: p.finalVolume || ""; s: qsTr("the fruit's juice makes up the rest") }
                        Row2 { k: qsTr("In the fermenter on brew day"); v: p.primaryGal || ""; s: qsTr("honey must plus any brew-day fruit") }
                        Row2 { visible: (p.bottled || "") !== ""; k: qsTr("What you'll likely bottle"); v: ""; s: p.bottled || "" }
                        Row2 { visible: p.backsweeten !== undefined; k: qsTr("Later: backsweeten"); v: ""; s: p.backsweeten || "" }
                        Label { text: qsTr("Yeast"); font.bold: true; color: Theme.ink; Layout.topMargin: 4 }
                        Body { text: p.yeast || "" }
                    }

                    Card {
                        visible: p.ready === true && p.reachable !== false && (p.more || []).length > 0
                        title: qsTr("Want more mead?")
                        Muted { text: qsTr("What to change for a bigger batch, as strong as this one. \"Same fruit\" adds honey and water; the other keeps the fruit as strong by adding fruit too. Press Plan it again after changing your goal's size or fruit.") }
                        Repeater {
                            model: p.more || []
                            delegate: Body { text: "• " + modelData }
                        }
                    }

                    Card {
                        visible: p.ready === true && (p.yeastOptions || []).length > 0
                        title: qsTr("Yeast options for this mead")
                        Repeater {
                            model: p.yeastOptions || []
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Label {
                                    text: modelData.mark
                                    Layout.alignment: Qt.AlignTop
                                    Layout.preferredWidth: 18
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: modelData.mark === "★" ? Theme.accent : modelData.mark === "✓" ? Theme.good
                                         : modelData.mark === "✗" ? Theme.warn : Theme.ink2
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Label {
                                        text: modelData.name + "  ·  " + modelData.label + (modelData.chosen ? qsTr("  (in this plan)") : "")
                                        color: Theme.ink
                                        font.bold: modelData.chosen
                                        font.pixelSize: 13
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                    Body { text: qsTr("Strength ") + modelData.strengthMark + "  " + modelData.strength; color: Theme.ink2; font.pixelSize: 12 }
                                    Body { text: qsTr("Flavour ") + modelData.flavourMark + "  " + modelData.flavour; color: Theme.ink2; font.pixelSize: 12 }
                                    Body { visible: modelData.extra !== ""; text: modelData.extra; color: Theme.ink3; font.pixelSize: 12 }
                                    Button {
                                        visible: !modelData.chosen && modelData.tolerance !== undefined
                                        text: modelData.reaches ? qsTr("Use %1").arg(modelData.code)
                                                                : qsTr("Use %1 at %2%").arg(modelData.code).arg(modelData.tolerance)
                                        onClicked: Designer.useYeast(modelData.id, modelData.reaches ? 0 : modelData.tolerance)
                                    }
                                }
                            }
                        }
                        Muted { text: qsTr("Strength comes from Piatz's tolerance table; flavour from the books' notes on each strain, matched to your fruit. ★ suggested (best of both, among dry yeasts) · ✓ good fit · * works, with a catch · ✗ not suitable · ? no tolerance figure. Every yeast in the Yeast list carries its mark.") }
                    }

                    Card {
                        visible: p.ready === true && (p.fit || []).length > 0
                        title: qsTr("Fitted to you")
                        Repeater {
                            model: p.fit || []
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: "•"; color: Theme.accent; Layout.alignment: Qt.AlignTop; font.pixelSize: 14 }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Body { text: modelData.text }
                                    Src { text: modelData.src; visible: text !== "" }
                                }
                            }
                        }
                    }

                    Card {
                        visible: p.ready === true
                        title: qsTr("Why, step by step")
                        headerExtras: [Button { id: whyBtn; checkable: true; text: checked ? qsTr("Hide") : qsTr("Show") }]
                        Repeater {
                            model: whyBtn.checked ? (p.why || []) : []
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: (index + 1) + "."; color: Theme.accent; Layout.alignment: Qt.AlignTop; font.pixelSize: 13 }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Body { text: modelData.text }
                                    Src { text: modelData.src; visible: text !== "" }
                                }
                            }
                        }
                    }

                    Card {
                        visible: p.ready === true
                        title: qsTr("Things to watch")
                        Repeater {
                            model: p.notes || []
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle {
                                    Layout.alignment: Qt.AlignTop
                                    Layout.topMargin: 6
                                    width: 8; height: 8; radius: 4
                                    color: modelData.level === "warn" ? Theme.warn : modelData.level === "good" ? Theme.good : Theme.info
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Body { text: modelData.text; color: modelData.level === "warn" ? Theme.ink : Theme.ink2 }
                                    Src { text: modelData.src; visible: text !== "" }
                                }
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 16 }
                }
            }
        }
    }

    SaveFirst { id: saveFirst }
}
