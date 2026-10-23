// Recipe designer: ingredients on the left, live results on the right (one column when the window is narrow).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeadWorkshop.Core

Item {
    id: page
    readonly property var d: Designer.design
    readonly property var r: Designer.results
    readonly property bool wide: width >= 1050
    readonly property var phases: [{ id: "primary", name: qsTr("Brew day (primary)") }, { id: "secondary", name: qsTr("Later, ~day 21 (secondary)") }]
    readonly property var whenChoices: [{ id: "", name: "—" }, { id: "primary", name: qsTr("Brew day (primary)") }, { id: "secondary", name: qsTr("Later, ~day 21 (secondary)") },
                                        { id: "aging", name: qsTr("After fermentation (aging)") }, { id: "bottling", name: qsTr("At bottling") }]
    readonly property var fruitList: Designer.fruitOptions()
    readonly property var juiceList: Designer.juiceOptions()
    readonly property var yeastList: Designer.yeastOptions()


    // ---------- small building blocks ----------
    component FieldLabel: Label { color: Theme.ink3; font.pixelSize: 12 }
    component Muted: Prose { color: Theme.ink3; font.pixelSize: 12; Layout.fillWidth: true }   // underlined terms open the word list
    component Body: Prose { color: Theme.ink2; Layout.fillWidth: true }
    component Src: Label { color: Theme.ink3; font.pixelSize: 11; font.italic: true }
    component RemoveButton: ToolButton {
        text: "×"
        font.pixelSize: 18
        Layout.alignment: Qt.AlignBottom
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Remove")
    }
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

    // ---------- header ----------
    RowLayout {
        id: head
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
        spacing: 8
        Label { text: qsTr("Recipe designer"); font.pixelSize: 20; font.bold: true; color: Theme.ink }
        Label {
            text: d.kind === "adapted" ? qsTr("Adapted recipe") : qsTr("Original recipe")
            color: d.kind === "adapted" ? Theme.info : Theme.good
            font.pixelSize: 12
            leftPadding: 8; rightPadding: 8; topPadding: 2; bottomPadding: 2
            background: Rectangle { radius: 9; color: "transparent"; border.color: parent.color }
        }
        Item { Layout.fillWidth: true }
        Label {
            id: toast
            text: Designer.message
            color: Theme.ink2
            opacity: 0
            Behavior on opacity { NumberAnimation { duration: 250 } }
            Connections {
                target: Designer
                function onMessageChanged() { toast.opacity = 1; toastTimer.restart() }
            }
            Timer { id: toastTimer; interval: 3000; onTriggered: toast.opacity = 0 }
        }
        Button { text: qsTr("New"); onClicked: saveFirst.ask(function() { Designer.newDesign() }) }
        Button {
            text: qsTr("Save")
            highlighted: Designer.unsaved
            onClicked: { page.commitName(); Designer.saveRecipe() }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Save in My recipes (%1)").arg(Designer.recipesFolder)
        }
        Button { text: qsTr("Save as…"); onClicked: { page.commitName(); saveAsDialog.open() } }
        Button { text: qsTr("Recipes…"); onClicked: recipesDialog.open() }
    }
    Label {
        id: savedLine
        anchors { left: parent.left; right: parent.right; top: head.bottom; leftMargin: 16; rightMargin: 16 }
        elide: Text.ElideRight
        font.pixelSize: 12
        color: Designer.unsaved ? Theme.warn : Theme.ink3
        text: Designer.savedAs === "" ? qsTr("Not saved yet: Save puts it in My recipes")
              : Designer.unsaved ? qsTr("“%1”: changes not saved yet").arg(Designer.savedAs)
              : qsTr("Saved in My recipes as “%1”").arg(Designer.savedAs)
    }
    // A name typed but not yet confirmed (Enter or clicking away) still counts when saving.
    function commitName() { if (nameField.text !== d.name) Designer.setField("name", nameField.text) }
    Shortcut { sequences: [StandardKey.Save]; onActivated: { page.commitName(); Designer.saveRecipe() } }   // Ctrl+S

    ScrollView {
        id: scroller
        anchors { left: parent.left; right: parent.right; top: savedLine.bottom; bottom: parent.bottom; topMargin: 8 }
        contentWidth: availableWidth

        GridLayout {
            id: grid
            width: scroller.availableWidth - 32
            x: 16
            columns: page.wide ? 2 : 1
            columnSpacing: 16
            rowSpacing: 14

            // ================================================================= LEFT: the recipe
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 14

                Card {
                    title: qsTr("Recipe")
                    Body {
                        visible: d.parentName !== ""
                        text: qsTr("Adapted from %1. The published version is never changed.").arg(d.parentName)
                        font.pixelSize: 13
                    }
                    GridLayout {
                        columns: 2
                        columnSpacing: 10
                        Layout.fillWidth: true
                        FieldLabel { text: qsTr("Name") }
                        FieldLabel { text: qsTr("Style") }
                        TextField {
                            id: nameField
                            Layout.fillWidth: true
                            text: d.name
                            selectByMouse: true
                            onEditingFinished: if (text !== d.name) Designer.setField("name", text)
                        }
                        Choice {
                            Layout.preferredWidth: 230
                            model: Designer.styleChoices()
                            current: d.style
                            onPicked: function(v) { Designer.setField("style", v) }
                        }
                    }
                    GridLayout {
                        columns: 3
                        columnSpacing: 10
                        Layout.fillWidth: true
                        FieldLabel { text: qsTr("Batch (gal)") }
                        FieldLabel { text: qsTr("Target sweetness") }
                        FieldLabel { text: qsTr("Brew date") }
                        NumField { value: d.volumeGal; onCommitted: function(t) { Designer.setField("volumeGal", t) } }
                        Choice {
                            Layout.fillWidth: true
                            model: Designer.sweetnessChoices()
                            textRole: "label"
                            valueRole: "key"
                            current: d.sweetness
                            onPicked: function(v) { Designer.setField("sweetness", v) }
                        }
                        TextField {
                            Layout.preferredWidth: 120
                            text: d.brewDate
                            placeholderText: "YYYY-MM-DD"
                            selectByMouse: true
                            onEditingFinished: if (text !== d.brewDate) Designer.setField("brewDate", text)
                        }
                    }
                    // What the style means (Piatz ch.2), and the mead in words.
                    Rectangle {
                        Layout.fillWidth: true
                        visible: r.style !== undefined
                        radius: 8
                        color: Theme.panel2
                        border.color: Theme.line
                        implicitHeight: styleCol.implicitHeight + 16
                        ColumnLayout {
                            id: styleCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 3
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: Theme.ink
                                font.bold: true
                                text: r.style ? r.style.name + (r.style.auto ? qsTr("  ·  from your ingredients: ") + r.style.because : "") : ""
                            }
                            Body { text: r.style ? r.style.plain : ""; font.pixelSize: 13 }
                            Body { text: r.style ? qsTr("In words: ") + r.style.words : ""; color: Theme.ink; font.pixelSize: 13 }
                            Muted { text: r.style ? qsTr("Strength: ") + r.style.strengthPlain + qsTr(". Sweetness: ") + r.style.sweetnessPlain + "." : "" }
                            Src { text: qsTr("Piatz ch.2 (BJCP categories)") }
                        }
                    }
                    CheckBox {
                        Layout.fillWidth: true
                        checked: d.countFruitWater
                        text: qsTr("Batch volume includes the water the fruit will release (Piatz's method). Untick if the volume is the base must and fruit adds on top.")
                        onToggled: Designer.setField("countFruitWater", checked)
                        contentItem: Label { text: parent.text; leftPadding: parent.indicator.width + 6; color: Theme.ink2; wrapMode: Text.WordWrap; font.pixelSize: 13 }
                    }
                }

                // ---------- Honey ----------
                Card {
                    title: qsTr("Honey")
                    headerExtras: [Button { text: qsTr("+ Honey"); onClicked: Designer.addRow("honeys") }]
                    Repeater {
                        model: d.honeys.length
                        delegate: RowLayout {
                            readonly property var row: d.honeys[index]
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                FieldLabel { text: qsTr("Variety") }
                                TextChoice {
                                    Layout.fillWidth: true
                                    model: Designer.honeyNames()
                                    current: row ? row.name : ""
                                    onCommitted: function(t) { Designer.setField("honeys." + index + ".name", t) }
                                }
                            }
                            ColumnLayout {
                                spacing: 2
                                FieldLabel { text: qsTr("Weight (lb)") }
                                NumField { value: row ? row.lb : ""; onCommitted: function(t) { Designer.setField("honeys." + index + ".lb", t) } }
                            }
                            RemoveButton { onClicked: Designer.removeRow("honeys", index) }
                        }
                    }
                    Muted { visible: d.honeys.length === 0; text: qsTr("No honey yet.") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        TextField { id: targetOG; placeholderText: qsTr("Target OG, e.g. 1.115"); Layout.fillWidth: true; selectByMouse: true; onAccepted: Designer.solveForOG(text) }
                        Button { text: qsTr("Solve honey for OG"); onClicked: Designer.solveForOG(targetOG.text) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        TextField { id: targetABV; placeholderText: qsTr("…or target ABV %, e.g. 13"); Layout.fillWidth: true; selectByMouse: true; onAccepted: Designer.solveForABV(text) }
                        Button { text: qsTr("Solve honey for ABV"); onClicked: Designer.solveForABV(targetABV.text) }
                    }
                    Muted { text: qsTr("Solving scales all the honeys together and keeps their proportions. The ABV solve uses your sweetness and yeast.") }
                }

                // ---------- Fruit ----------
                Card {
                    title: qsTr("Fruit")
                    headerExtras: [Button { text: qsTr("+ Fruit"); onClicked: Designer.addRow("fruits") }]
                    Repeater {
                        model: d.fruits.length
                        delegate: ColumnLayout {
                            readonly property var row: d.fruits[index]
                            Layout.fillWidth: true
                            spacing: 2
                            GridLayout {
                                columns: 6
                                columnSpacing: 8
                                rowSpacing: 2
                                Layout.fillWidth: true
                                FieldLabel { text: qsTr("Fruit") }
                                FieldLabel { text: qsTr("lb") }
                                FieldLabel { text: qsTr("Added") }
                                FieldLabel { text: qsTr("Water %") }
                                FieldLabel { text: qsTr("Sugar %") }
                                Item { width: 1 }
                                Choice {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 150
                                    model: page.fruitList
                                    current: row ? row.fruitId : ""
                                    onPicked: function(v) { Designer.setField("fruits." + index + ".fruitId", v) }
                                }
                                NumField { Layout.preferredWidth: 70; value: row ? row.lb : ""; onCommitted: function(t) { Designer.setField("fruits." + index + ".lb", t) } }
                                Choice {
                                    Layout.preferredWidth: 120
                                    model: page.phases
                                    current: row ? row.phase : "primary"
                                    onPicked: function(v) { Designer.setField("fruits." + index + ".phase", v) }
                                }
                                NumField { Layout.preferredWidth: 64; value: row ? row.water : ""; onCommitted: function(t) { Designer.setField("fruits." + index + ".water", t) } }
                                NumField { Layout.preferredWidth: 64; value: row ? row.sugar : ""; placeholderText: "n/a"; onCommitted: function(t) { Designer.setField("fruits." + index + ".sugar", t) } }
                                RemoveButton { onClicked: Designer.removeRow("fruits", index) }
                            }
                            Muted { visible: text !== ""; text: row ? row.meta : ""; font.pixelSize: 11 }
                        }
                    }
                    Muted { visible: d.fruits.length === 0; text: qsTr("None. No fruit makes a traditional mead.") }
                }

                // ---------- Juice ----------
                Card {
                    title: qsTr("Juice & concentrate")
                    headerExtras: [Button { text: qsTr("+ Juice"); onClicked: Designer.addRow("juices") }]
                    Repeater {
                        model: d.juices.length
                        delegate: ColumnLayout {
                            readonly property var row: d.juices[index]
                            Layout.fillWidth: true
                            spacing: 2
                            GridLayout {
                                columns: 6
                                columnSpacing: 8
                                rowSpacing: 2
                                Layout.fillWidth: true
                                FieldLabel { text: qsTr("Juice") }
                                FieldLabel { text: qsTr("gal") }
                                FieldLabel { text: row && row.sgEstimated ? qsTr("SG (est.)") : qsTr("SG") }
                                FieldLabel { text: qsTr("Conc. ×") }
                                FieldLabel { text: qsTr("Added") }
                                Item { width: 1 }
                                Choice {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 150
                                    model: page.juiceList
                                    current: row ? row.juiceId : ""
                                    onPicked: function(v) { Designer.setField("juices." + index + ".juiceId", v) }
                                }
                                NumField { Layout.preferredWidth: 64; value: row ? row.gal : ""; onCommitted: function(t) { Designer.setField("juices." + index + ".gal", t) } }
                                NumField { Layout.preferredWidth: 72; value: row ? row.sg : ""; placeholderText: qsTr("measure"); onCommitted: function(t) { Designer.setField("juices." + index + ".sg", t) } }
                                NumField { Layout.preferredWidth: 52; value: row ? row.ratio : ""; onCommitted: function(t) { Designer.setField("juices." + index + ".ratio", t) } }
                                Choice {
                                    Layout.preferredWidth: 120
                                    model: page.phases
                                    current: row ? row.phase : "primary"
                                    onPicked: function(v) { Designer.setField("juices." + index + ".phase", v) }
                                }
                                RemoveButton { onClicked: Designer.removeRow("juices", index) }
                            }
                            Muted { visible: text !== ""; text: row ? row.meta : ""; font.pixelSize: 11 }
                        }
                    }
                    Muted { visible: d.juices.length === 0; text: qsTr("None.") }
                }

                // ---------- Other fermentables ----------
                Card {
                    title: qsTr("Other fermentables")
                    headerExtras: [Button { text: qsTr("+ Sugar"); onClicked: Designer.addRow("sugars") }]
                    Repeater {
                        model: d.sugars.length
                        delegate: GridLayout {
                            readonly property var row: d.sugars[index]
                            columns: 5
                            columnSpacing: 8
                            rowSpacing: 2
                            Layout.fillWidth: true
                            FieldLabel { text: qsTr("Ingredient") }
                            FieldLabel { text: qsTr("lb") }
                            FieldLabel { text: qsTr("Points/lb/gal") }
                            FieldLabel { text: qsTr("Added") }
                            Item { width: 1 }
                            TextChoice {
                                Layout.fillWidth: true
                                model: Designer.sugarNames()
                                current: row ? row.name : ""
                                onCommitted: function(t) { Designer.setField("sugars." + index + ".name", t) }
                            }
                            NumField { Layout.preferredWidth: 64; value: row ? row.lb : ""; onCommitted: function(t) { Designer.setField("sugars." + index + ".lb", t) } }
                            NumField { Layout.preferredWidth: 64; value: row ? row.ppg : ""; onCommitted: function(t) { Designer.setField("sugars." + index + ".ppg", t) } }
                            Choice {
                                Layout.preferredWidth: 120
                                model: page.phases
                                current: row ? row.phase : "primary"
                                onPicked: function(v) { Designer.setField("sugars." + index + ".phase", v) }
                            }
                            RemoveButton { onClicked: Designer.removeRow("sugars", index) }
                        }
                    }
                    Muted { visible: d.sugars.length === 0; text: qsTr("None.") }
                }

                // ---------- Adjuncts ----------
                Card {
                    title: qsTr("Spices, zest, oak & other adjuncts")
                    headerExtras: [Button { text: qsTr("+ Adjunct"); onClicked: Designer.addRow("adjuncts") }]
                    Repeater {
                        model: d.adjuncts.length
                        delegate: GridLayout {
                            readonly property var row: d.adjuncts[index]
                            columns: 4
                            columnSpacing: 8
                            rowSpacing: 2
                            Layout.fillWidth: true
                            FieldLabel { text: qsTr("Ingredient") }
                            FieldLabel { text: qsTr("Amount") }
                            FieldLabel { text: qsTr("When") }
                            Item { width: 1 }
                            TextChoice {
                                Layout.fillWidth: true
                                model: Designer.adjunctNames()
                                current: row ? row.name : ""
                                onCommitted: function(t) { Designer.setField("adjuncts." + index + ".name", t) }
                            }
                            TextField {
                                Layout.preferredWidth: 120
                                text: row ? row.amount : ""
                                selectByMouse: true
                                onEditingFinished: if (row && text !== row.amount) Designer.setField("adjuncts." + index + ".amount", text)
                            }
                            Choice {
                                Layout.preferredWidth: 120
                                model: page.whenChoices
                                current: row ? row.when : ""
                                onPicked: function(v) { Designer.setField("adjuncts." + index + ".when", v) }
                            }
                            RemoveButton { onClicked: Designer.removeRow("adjuncts", index) }
                            Muted {
                                Layout.columnSpan: 4
                                Layout.fillWidth: true
                                visible: text !== ""
                                font.pixelSize: 11
                                text: Designer.spiceGuide(row ? row.name : "", d.volumeGal)
                            }
                        }
                    }
                    Muted { visible: d.adjuncts.length === 0; text: qsTr("None. Adjuncts are logged for the recipe; they don't change the gravity math.") }
                }

                // ---------- Yeast ----------
                Card {
                    title: qsTr("Yeast")
                    GridLayout {
                        columns: 2
                        columnSpacing: 10
                        Layout.fillWidth: true
                        FieldLabel { text: qsTr("Strain") }
                        FieldLabel { text: qsTr("Tolerance override %") }
                        Choice {
                            Layout.fillWidth: true
                            model: page.yeastList
                            current: d.yeastId
                            onPicked: function(v) { Designer.setField("yeastId", v) }
                        }
                        NumField { value: d.yeastTolerance; placeholderText: qsTr("from table"); onCommitted: function(t) { Designer.setField("yeastTolerance", t) } }
                    }
                    Body { text: r.yeastAmount || ""; visible: text !== ""; color: Theme.ink; font.pixelSize: 13 }
                    Body { text: r.yeastInfo; font.pixelSize: 13 }
                    Src { text: r.yeastSrc; visible: text !== "" }
                }

                // ---------- Notes ----------
                Card {
                    title: qsTr("Notes")
                    TextArea {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(80, implicitHeight)
                        text: d.notes
                        wrapMode: TextEdit.Wrap
                        placeholderText: qsTr("Intent, substitutions, ideas…")
                        onActiveFocusChanged: if (!activeFocus && text !== d.notes) Designer.setField("notes", text)
                    }
                }
            }

            // ================================================================= RIGHT: results
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 14

                GridLayout {
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 8
                    Layout.fillWidth: true
                    Stat { k: qsTr("Brew-day reading (mixing OG)"); term: "mixingOG"; v: Theme.sg3(r.mixingOG); s: qsTr("what the hydrometer should read, before whole fruit") }
                    Stat { k: qsTr("Starting sugar (effective OG)"); term: "effOG"; v: Theme.sg3(r.effectiveOG); s: qsTr("counting all the fruit and additions"); hl: true }
                    Stat {
                        k: qsTr("Ending sugar (FG)"); term: "fg"; v: Theme.sg3(r.fg)
                        s: (r.fgLow !== r.fgHigh ? Theme.sg3(r.fgLow) + "–" + Theme.sg3(r.fgHigh) + qsTr(" (yeast range) · ") : "") + r.sweetnessLabel
                    }
                    Stat { k: qsTr("Alcohol (ABV)"); term: "abv"; v: Number(r.abv).toFixed(1) + "%"; s: r.strength + qsTr(" · chemistry check ≈ ") + Number(r.abvBalling).toFixed(1) + "%" }
                    Stat { k: qsTr("Water to pour in"); v: Number(r.waterToAdd).toFixed(2) + " gal"; s: qsTr("all the water. With the honey in, the level comes up to %1 gal (not more water)").arg(Number(r.preFruit).toFixed(2)) }
                    Stat { k: qsTr("Final volume"); term: "fruitWater"; v: Number(r.finalVolume).toFixed(2) + " gal"; s: qsTr("fruit water %1 gal").arg(Number(r.fruitWater).toFixed(2)) }
                }

                // ---------- Profile ----------
                Card {
                    title: qsTr("Profile")
                    headerExtras: [Label { text: qsTr("▮ predicted · ◆ target / yeast limit"); color: Theme.ink3; font.pixelSize: 11 }]

                    Button {
                        id: explainBtn
                        flat: true
                        checkable: true
                        text: checked ? qsTr("▾ What does this mean?") : qsTr("▸ What does this mean?")
                        padding: 2
                    }
                    Label {
                        visible: explainBtn.checked
                        color: Theme.ink2
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        textFormat: Text.StyledText
                        text: qsTr("<b>Sweetness</b> is how much sugar is left when the yeast is done. The dark line shows where this recipe should end up, from dry (no sweetness) to sweet. Drag the slider to the sweetness you want and you get two ways to get there: <b>A</b> changes how much honey goes in so the yeast \"runs out of steam\" right at your target; <b>B</b> lets the yeast eat everything, then you stop it and stir honey back in to taste. B is easier to get exactly right.<br><br>" +
                                   "<b>Strength</b> is the alcohol. Every yeast can only survive up to a certain alcohol level (the ◆). If there's more sugar than the yeast can handle, the leftover sugar becomes sweetness.<br><br>" +
                                   "<b>Fruit bars</b> show how strongly each fruit will come through, based on how much fruit you're using for your batch size. Drag a slider to use more or less fruit. With the box ticked, the honey is adjusted so the alcohol stays about the same.")
                    }

                    // Sweetness
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Sweetness"); font.bold: true; color: Theme.ink }
                        Item { Layout.fillWidth: true }
                        Label { text: Theme.sg3(r.fg) + " · " + r.sweetnessLabel; color: Theme.ink2 }
                    }
                    Meter {
                        Layout.fillWidth: true
                        minV: 0.990; maxV: 1.050
                        zones: r.sweetZones
                        value: r.fg
                        band: r.fgLow !== r.fgHigh ? [r.fgLow, r.fgHigh] : undefined
                        target: sweetSlider.pressed ? sweetSlider.value : r.targetFG
                    }
                    Src { text: qsTr("│ line = where the yeast should stop (predicted FG) · ◆ = the sweetness you want (the slider). Backsweetening closes the gap.") }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Target ending sugar (FG)"); color: Theme.ink2 }
                        Label { text: Number(sweetSlider.value).toFixed(3); font.bold: true; color: Theme.accent }
                        Slider {
                            id: sweetSlider
                            Layout.fillWidth: true
                            from: 0.990; to: 1.050; stepSize: 0.001
                            focusPolicy: Qt.NoFocus      // don't take focus from a text box (that would save it and reset the slider)
                            enabled: !d.wild
                            property var preview: []
                            // Follow the recipe, but never while the knob is being dragged.
                            function sync() { if (!pressed) value = page.r.targetFG }
                            Component.onCompleted: sync()
                            Connections { target: Designer; function onChanged() { sweetSlider.sync() } }
                            onMoved: { if (pressed) preview = Designer.previewSweetOptions(value); else Designer.setTargetFG(value) }
                            onPressedChanged: {
                                if (pressed) preview = Designer.previewSweetOptions(value)
                                else Designer.setTargetFG(value)
                            }
                        }
                    }
                    Repeater {
                        model: sweetSlider.pressed ? sweetSlider.preview : r.sweetOptions
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: optRow.implicitHeight + 14
                            radius: 8
                            color: Theme.panel2
                            border.color: modelData.kind === "ok" ? Theme.good : Theme.line
                            RowLayout {
                                id: optRow
                                anchors.fill: parent
                                anchors.margins: 7
                                spacing: 8
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label { visible: modelData.title !== ""; text: modelData.title; font.bold: true; color: Theme.ink }
                                    Label { text: modelData.text; color: modelData.kind === "balance" ? Theme.ink3 : Theme.ink2; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 13 }
                                    Src { text: modelData.src }
                                }
                                Button {
                                    visible: modelData.applyOG !== undefined && !sweetSlider.pressed
                                    text: qsTr("Apply")
                                    onClicked: Designer.applyOption(modelData.applyOG, sweetSlider.value)
                                }
                                Button {
                                    visible: modelData.backsweetenLb !== undefined && !sweetSlider.pressed
                                    text: qsTr("Add to plan")
                                    onClicked: Designer.addBacksweetening(modelData.backsweetenLb, sweetSlider.value)
                                }
                            }
                        }
                    }
                    Src { text: r.scaleName + qsTr(" scale · ") + r.scaleSrc }

                    // Strength
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        Label { text: qsTr("Strength"); font.bold: true; color: Theme.ink }
                        Item { Layout.fillWidth: true }
                        Label { text: Number(r.abv).toFixed(1) + "% · " + r.strength; color: Theme.ink2 }
                    }
                    Meter {
                        Layout.fillWidth: true
                        minV: 0; maxV: 20
                        zones: r.strengthZones
                        value: r.abv
                        target: r.tolerance
                    }
                    Src { text: qsTr("◆ = yeast tolerance %1% · Piatz ch.2").arg(r.tolerance) }

                    // Fruit
                    Repeater {
                        model: r.fruitBars
                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.name; font.bold: true; color: Theme.ink }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: fruitSlider.pressed ? Designer.previewFruitLevel(modelData.index, fruitSlider.value) : modelData.label
                                    color: Theme.ink2
                                }
                            }
                            Meter {
                                Layout.fillWidth: true
                                minV: 0; maxV: modelData.max
                                zones: modelData.zones
                                value: fruitSlider.value
                            }
                            Slider {
                                id: fruitSlider
                                Layout.fillWidth: true
                                from: 0; to: modelData.max; stepSize: 0.25
                                focusPolicy: Qt.NoFocus
                                Component.onCompleted: value = modelData.lb
                                onPressedChanged: {
                                    if (!pressed && value !== modelData.lb) Designer.setFruitAmount(modelData.index, value)
                                }
                                onMoved: if (!pressed) Designer.setFruitAmount(modelData.index, value)
                            }
                            Src { text: modelData.src }
                        }
                    }
                    CheckBox {
                        visible: r.fruitBars.length > 0
                        Layout.fillWidth: true
                        checked: Designer.holdOG
                        text: qsTr("When I change fruit, re-solve the honey so the effective OG stays at %1").arg(Theme.sg3(r.effectiveOG))
                        onToggled: Designer.holdOG = checked
                        contentItem: Label { text: parent.text; leftPadding: parent.indicator.width + 6; color: Theme.ink2; wrapMode: Text.WordWrap; font.pixelSize: 13 }
                    }
                }

                // ---------- Published original ----------
                Card {
                    visible: r.published !== undefined
                    title: qsTr("Faithfulness to the original")
                    headerExtras: [Label { text: r.faithfulness ? r.faithfulness.level : ""; color: r.faithfulness && r.faithfulness.changes.length ? Theme.info : Theme.good; font.bold: true }]
                    Body { text: r.published ? qsTr("%1 — %2. Published figures: %3").arg(r.published.name).arg(r.published.ref).arg(r.published.figures) : ""; font.pixelSize: 13 }
                    Body { visible: r.published !== undefined && r.published.notes !== ""; text: r.published ? r.published.notes : ""; font.pixelSize: 13 }
                    Muted { text: qsTr("The app uses the books' 40 points/lb/gal. Piatz's printed recipes run about 2 points per pound higher; switch to 42 in Settings to match them exactly.") }
                    Repeater {
                        model: r.faithfulness ? r.faithfulness.changes : []
                        delegate: Body { text: "• " + modelData; font.pixelSize: 13 }
                    }
                    Muted {
                        visible: r.faithfulness !== undefined && r.faithfulness.changes.length === 0
                        text: r.faithfulness && Math.abs(r.faithfulness.scale - 1) > 0.001
                              ? qsTr("Same ingredients and proportions, scaled ×%1.").arg(Number(r.faithfulness.scale).toFixed(2))
                              : qsTr("Same ingredients and proportions.")
                    }
                }

                // ---------- Review ----------
                Card {
                    title: qsTr("Review")
                    Repeater {
                        model: r.notes
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
                                Body { text: modelData.text; color: modelData.level === "warn" ? Theme.ink : Theme.ink2; font.pixelSize: 13 }
                                Src { text: modelData.src; visible: text !== "" }
                            }
                        }
                    }
                    Body { visible: r.notes.length === 0; text: qsTr("Nothing to flag."); color: Theme.good }
                }

                // ---------- Fruit character ----------
                Card {
                    visible: r.fruitCharacter.length > 0
                    title: qsTr("Fruit character")
                    Repeater {
                        model: r.fruitCharacter
                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.names; color: Theme.ink; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                Label { text: modelData.level; color: Theme.accent; font.bold: true }
                            }
                            Muted { text: modelData.basis + (modelData.note !== "" ? " " + modelData.note : "") }
                            Src { text: modelData.src }
                        }
                    }
                }

                // ---------- Nutrients ----------
                Card {
                    visible: r.nutrients !== undefined
                    title: qsTr("Yeast food (nutrients)")
                    headerExtras: [Src { text: "Piatz ch.4–5" }]
                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 4
                        Layout.fillWidth: true
                        FieldLabel { text: qsTr("Yeast") }
                        Body { text: r.nutrients ? r.nutrients.packets + (r.nutrients.packets > 1 ? qsTr(" packets") : qsTr(" packet")) : ""; font.pixelSize: 13 }
                        FieldLabel { text: qsTr("Go-Ferm"); Layout.alignment: Qt.AlignTop }
                        Body { text: r.nutrients ? r.nutrients.goFerm : ""; font.pixelSize: 13 }
                        FieldLabel { text: qsTr("Mix in a bag") }
                        Body { text: r.nutrients ? r.nutrients.mix : ""; font.pixelSize: 13 }
                    }
                    Repeater {
                        model: r.nutrients ? r.nutrients.doses : []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Body { text: modelData.when; font.pixelSize: 13 }
                            Label { text: modelData.amount; color: Theme.accent }
                        }
                    }
                    Muted { text: r.nutrients ? r.nutrients.caveat : "" }
                    Muted { text: r.nutrients ? r.nutrients.schramm : "" }
                    Muted { text: r.nutrients ? r.nutrients.chemistry : "" }
                }

                // ---------- The math ----------
                Card {
                    title: qsTr("Show the math")
                    headerExtras: [Button { id: mathBtn; checkable: true; text: checked ? qsTr("Hide") : qsTr("Show") }]
                    Repeater {
                        model: mathBtn.checked ? r.math : []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Label { text: modelData.k; color: Theme.ink3; Layout.preferredWidth: 130; Layout.alignment: Qt.AlignTop; font.pixelSize: 13 }
                            Body { text: modelData.v; font.pixelSize: 13 }
                        }
                    }
                    Muted {
                        visible: mathBtn.checked
                        text: qsTr("Formulas from Piatz ch.3 and ch.9 (fruit water and sugar, NS, BOG, yeast-limit table, ABV) and the 40-point honey rule (Piatz ch.9 chart; Schramm ch.5).")
                    }
                }
                Item { Layout.preferredHeight: 16 }
            }
        }
    }

    // ---------- Recipes: my saved recipes and the books' recipes ----------
    SaveFirst { id: saveFirst }

    Dialog {
        id: recipesDialog
        title: qsTr("Recipes")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 40, 600)
        height: Math.min(page.height - 40, 640)
        standardButtons: Dialog.Close
        property var recipes: Designer.publishedRecipes()
        property int picked: -1
        property var mine: []
        property int minePicked: -1
        function refresh() { mine = Designer.myRecipes(); minePicked = -1 }
        onAboutToShow: { refresh(); picked = -1; tabs.currentIndex = mine.length > 0 ? 0 : recipes.some(r => r.full) ? 1 : 0 }
        background: Rectangle { color: Theme.panel; border.color: Theme.line; radius: 10 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            TabBar {
                id: tabs
                Layout.fillWidth: true
                TabButton { text: qsTr("My recipes (%1)").arg(recipesDialog.mine.length) }
                TabButton { text: qsTr("From the books") }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: tabs.currentIndex

                // ---- My recipes ----
                ColumnLayout {
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.ink3
                        font.pixelSize: 12
                        text: qsTr("Saved in %1").arg(Designer.recipesFolder)
                    }
                    Label {
                        visible: recipesDialog.mine.length === 0
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.ink2
                        text: qsTr("No saved recipes yet. Click Save in the Designer and the recipe on screen is kept here under its name.")
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: recipesDialog.mine
                        currentIndex: recipesDialog.minePicked
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem
                            contentItem: ColumnLayout {
                                spacing: 0
                                Label {
                                    text: modelData.name + (modelData.file === Designer.savedAs ? qsTr("  (open now)") : "")
                                    color: Theme.ink; font.bold: true
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                }
                                Label {
                                    text: !modelData.ok ? qsTr("Can't be read (the file is damaged)")
                                          : [modelData.style, modelData.volumeGal > 0 ? modelData.volumeGal + " gal" : "",
                                             modelData.kind === "adapted" ? qsTr("adapted") : "", qsTr("saved %1").arg(modelData.saved)]
                                            .filter(x => x !== "").join(" · ")
                                    color: modelData.ok ? Theme.ink3 : Theme.warn
                                    font.pixelSize: 12
                                }
                            }
                            onClicked: recipesDialog.minePicked = index
                            onDoubleClicked: openBtn.clicked()
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: qsTr("Open the folder"); onClicked: Designer.showRecipesFolder() }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: qsTr("Delete…")
                            enabled: recipesDialog.minePicked >= 0
                            onClicked: { deleteDialog.entry = recipesDialog.mine[recipesDialog.minePicked]; deleteDialog.open() }
                        }
                        Button {
                            id: openBtn
                            text: qsTr("Open")
                            highlighted: true
                            enabled: recipesDialog.minePicked >= 0 && recipesDialog.mine[recipesDialog.minePicked].ok
                            onClicked: {
                                if (!enabled) return
                                const file = recipesDialog.mine[recipesDialog.minePicked].file
                                recipesDialog.close()
                                if (file === Designer.savedAs && !Designer.unsaved) return
                                saveFirst.ask(function() { Designer.openRecipe(file) })
                            }
                        }
                    }
                }

                // ---- From the books ----
                ColumnLayout {
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.ink2
                        text: recipesDialog.recipes.some(r => r.full)
                              ? qsTr("This makes an adapted copy you can change. The published recipe stays exactly as printed, and the Designer shows how far your copy has moved from it.")
                              : qsTr("Mead Workshop lists the books' recipes by name and chapter only, so you can find them in your copy. If you own the books, a book pack (book-pack/recipes.json next to the app) adds the full recipes here.")
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: recipesDialog.recipes
                        currentIndex: recipesDialog.picked
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem
                            contentItem: ColumnLayout {
                                spacing: 0
                                Label { text: modelData.name; color: Theme.ink; font.bold: true }
                                Label { text: modelData.author + " · " + modelData.style + " · " + modelData.volumeGal + " gal"; color: Theme.ink3; font.pixelSize: 12 }
                                Label { visible: !modelData.full; text: qsTr("Listed by name only: see %1, %2").arg(modelData.author).arg(modelData.ref); color: Theme.ink3; font.pixelSize: 11; font.italic: true }
                            }
                            onClicked: { recipesDialog.picked = index; scaleTo.text = String(modelData.volumeGal) }
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Scale to (gal)"); color: Theme.ink2 }
                        TextField { id: scaleTo; Layout.preferredWidth: 80; selectByMouse: true; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: qsTr("Adapt in Designer")
                            enabled: recipesDialog.picked >= 0 && recipesDialog.recipes[recipesDialog.picked].full
                            highlighted: true
                            onClicked: {
                                const rec = recipesDialog.recipes[recipesDialog.picked]
                                const v = Number(scaleTo.text.replace(",", "."))
                                recipesDialog.close()
                                saveFirst.ask(function() { Designer.adaptPublished(rec.id, v > 0 ? v : rec.volumeGal) })
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: saveAsDialog
        title: qsTr("Save a copy as")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 40, 440)
        standardButtons: Dialog.Save | Dialog.Cancel
        background: Rectangle { color: Theme.panel; border.color: Theme.line; radius: 10 }
        onAboutToShow: { saveAsName.text = d.name; saveAsName.selectAll(); saveAsName.forceActiveFocus() }
        onAccepted: Designer.saveRecipeAs(saveAsName.text)
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink2
                text: qsTr("Saves the recipe under a new name in My recipes. The one you started from stays as it was.")
            }
            TextField { id: saveAsName; Layout.fillWidth: true; selectByMouse: true; onAccepted: saveAsDialog.accept() }
        }
    }

    Dialog {
        id: deleteDialog
        property var entry: ({})
        title: qsTr("Delete this recipe?")
        modal: true
        anchors.centerIn: parent
        width: Math.min(page.width - 40, 440)
        standardButtons: Dialog.Yes | Dialog.Cancel
        background: Rectangle { color: Theme.panel; border.color: Theme.line; radius: 10 }
        onAccepted: { Designer.deleteRecipe(entry.file); recipesDialog.refresh() }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.ink2
            text: qsTr("“%1” goes to %2, so you can still get it back from there.").arg(deleteDialog.entry.name || "").arg(Qt.platform.os === "windows" ? qsTr("the Recycle Bin") : qsTr("the Trash"))
        }
    }
}
