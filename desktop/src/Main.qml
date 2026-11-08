import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import ArchSign 1.0

ApplicationWindow {
    id: win
    width: 1040; height: 860
    minimumWidth: 860; minimumHeight: 820
    visible: true
    title: qsTr("Arch sign")
    color: theme.colors.background
    Material.theme: theme.light ? Material.Light : Material.Dark
    Material.background: theme.colors.surface
    Material.foreground: theme.colors.foreground
    Material.primary: theme.colors.surface
    palette.window: theme.colors.background
    palette.windowText: theme.colors.foreground
    palette.text: theme.colors.foreground
    palette.buttonText: theme.colors.foreground
    palette.base: theme.colors.background
    palette.button: theme.colors.surface
    palette.highlight: theme.accent
    palette.highlightedText: theme.accentForeground
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    property bool helpVisible: false
    property string pendingAction: ""
    function requestAction(action) {
        if (backend.dirty) { pendingAction = action; discard.open() }
        else runAction(action)
    }
    function runAction(action) {
        if (action === "quit") Qt.quit()
        else if (action === "manual") backend.useOwnColors()
        else if (action === "theme") backend.followTheme()
        else if (action === "off") backend.turnOff()
        else if (action === "reset") backend.resetTheme(backend.activeTheme)
    }
    onClosing: function(close) {
        if (backend.dirty) { close.accepted = false; requestAction("quit") }
    }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.requestAction("quit") }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: { if (backend.mode === "manual") backend.apply() } }
    Shortcut { sequence: "Ctrl+Return"; context: Qt.ApplicationShortcut; onActivated: { if (backend.mode === "manual") backend.apply() } }
    Connections {
        target: backend
        function onScreenPickFinished() { win.show(); win.raise(); win.requestActivate() }
    }
    ColorDialog {
        id: picker
        property bool archTarget: true
        title: archTarget ? qsTr("Arch color") : qsTr("Omarchy color")
        options: ColorDialog.DontUseNativeDialog
        onAccepted: {
            let hex = selectedColor.toString().slice(0, 7)
            if (archTarget) backend.arch = hex
            else backend.omarchy = hex
        }
    }
    Dialog {
        id: discard
        title: qsTr("Discard unapplied changes?")
        anchors.centerIn: parent
        standardButtons: Dialog.Discard | Dialog.Cancel
        onDiscarded: win.runAction(win.pendingAction)
    }
    Dialog {
        id: help
        objectName: "helpDialog"
        title: qsTr("Keyboard shortcuts")
        anchors.centerIn: parent
        standardButtons: Dialog.Ok
        Label { text: qsTr("Space / Ctrl+Enter: apply colors\nQ: quit\n?: keyboard shortcuts") }
    }
    function chooseColor(arch) {
        picker.archTarget = arch
        picker.selectedColor = arch ? backend.arch : backend.omarchy
        picker.open()
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 18
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 5
                Label { text: qsTr("Arch sign"); font.pixelSize: 26; font.bold: true }
                Label { text: qsTr("A little light. Your own atmosphere."); color: theme.colors.secondary; font.pixelSize: 13 }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                implicitWidth: modeLabel.implicitWidth + 28; implicitHeight: 32
                color: theme.colors.surface; radius: theme.radius; border.color: theme.colors.border
                Label {
                    id: modeLabel; anchors.centerIn: parent; font.pixelSize: 12
                    color: theme.accent
                    text: backend.mode === "theme" ? qsTr("Following the desktop theme") : backend.mode === "manual" ? qsTr("Own colors") : qsTr("Sign switched off")
                }
            }
            Rectangle {
                implicitWidth: themeOrigin.implicitWidth + 28; implicitHeight: 32
                color: theme.colors.surface; radius: theme.radius; border.color: theme.colors.border
                Label {
                    id: themeOrigin
                    objectName: "themeOriginLabel"
                    anchors.centerIn: parent; font.pixelSize: 12
                    color: theme.colors.foreground
                    text: backend.hasThemePreset ? qsTr("Theme colors: Custom") : qsTr("Theme colors: Automatic")
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 24
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: theme.colors.preview; radius: theme.radius; border.color: theme.colors.border
                clip: true
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("YOUR SIGN"); font.pixelSize: 11; font.letterSpacing: 2; color: theme.colors.secondary }
                        Item { Layout.fillWidth: true }
                        Label { text: backend.dirty ? qsTr("Unapplied preview") : qsTr("Preview"); font.pixelSize: 11; color: backend.dirty ? theme.accent : theme.colors.secondary }
                    }
                    SignPreview {
                        objectName: "signPreview"
                        Layout.fillWidth: true; Layout.fillHeight: true
                        archColor: /^#[0-9a-fA-F]{6}$/.test(backend.arch) ? backend.arch : "#000000"
                        omarchyColor: /^#[0-9a-fA-F]{6}$/.test(backend.omarchy) ? backend.omarchy : "#000000"
                        brightness: backend.brightness
                        lit: backend.mode !== "off"
                    }
                    Label {
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Backlight around the edges · illustrative preview")
                        font.pixelSize: 11; color: theme.colors.subtle; wrapMode: Text.Wrap
                    }
                }
            }
            ColumnLayout {
                Layout.preferredWidth: 300; Layout.maximumWidth: 330
                Layout.fillHeight: true; spacing: 10
                Label { text: qsTr("Lighting"); font.pixelSize: 20; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true; spacing: 0
                    RadioButton {
                        Layout.preferredHeight: 36; objectName: "manualMode"; text: qsTr("Own colors")
                        checked: backend.mode === "manual"; autoExclusive: false
                        onClicked: win.requestAction("manual")
                    }
                    RadioButton {
                        Layout.preferredHeight: 36; objectName: "themeButton"; text: qsTr("Follow theme")
                        checked: backend.mode === "theme"; autoExclusive: false
                        onClicked: win.requestAction("theme")
                    }
                }
                Repeater {
                    model: ["Arch", "Omarchy"]
                    delegate: Rectangle {
                        required property string modelData
                        Layout.fillWidth: true; implicitHeight: 123
                        color: theme.colors.surface; border.color: theme.colors.border; radius: theme.radius
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 14; spacing: 8
                            RowLayout {
                                Layout.fillWidth: true
                                Rectangle {
                                    implicitWidth: 12; implicitHeight: 12; radius: Math.min(4, theme.radius)
                                    color: (modelData === "Arch" ? backend.arch : backend.omarchy).match(/^#[0-9a-fA-F]{6}$/) ? (modelData === "Arch" ? backend.arch : backend.omarchy) : theme.colors.surface
                                }
                                Label { text: modelData; font.pixelSize: 16; font.bold: true }
                                Item { Layout.fillWidth: true }
                                Button {
                                    Layout.preferredHeight: 32; text: qsTr("Choose color"); flat: true; enabled: backend.mode === "manual"
                                    onClicked: win.chooseColor(modelData === "Arch")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                TextField {
                                    Layout.fillWidth: true; Layout.preferredHeight: 48; enabled: backend.mode === "manual"
                                    text: modelData === "Arch" ? backend.arch : backend.omarchy
                                    placeholderText: qsTr("Hex color"); maximumLength: 7
                                    onTextEdited: {
                                        if (modelData === "Arch") backend.arch = text
                                        else backend.omarchy = text
                                    }
                                }
                                Button {
                                    objectName: modelData === "Arch" ? "archPipette" : "omarchyPipette"
                                    text: ""
                                    icon.source: "qrc:/pipette.svg"
                                    icon.width: 19; icon.height: 19
                                    icon.color: theme.colors.foreground
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 48
                                    Accessible.name: qsTr("Pick pixel")
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Pick pixel")
                                    enabled: backend.mode === "manual"
                                    Accessible.description: qsTr("Choose a color anywhere on your desktop")
                                    onClicked: {
                                        if (backend.pickScreenColor(modelData === "Arch")) win.hide()
                                    }
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Brightness"); color: theme.colors.foreground }
                    Item { Layout.fillWidth: true }
                    Label { text: { let language = translator.language; return Math.round(backend.brightness / 255 * 100).toLocaleString(Qt.locale(), 'f', 0) + " %" } color: theme.colors.secondary }
                }
                Slider {
                    Layout.fillWidth: true; Layout.preferredHeight: 32; enabled: backend.mode === "manual"
                    from: 0; to: 255; stepSize: 1; value: backend.brightness
                    onMoved: backend.brightness = Math.round(value)
                }
                Button {
                    Layout.fillWidth: true; Layout.preferredHeight: 44; objectName: "applyButton"; enabled: backend.mode === "manual"
                    text: qsTr("Apply manual colors"); highlighted: true; onClicked: backend.apply()
                }
                Button {
                    Layout.fillWidth: true; Layout.preferredHeight: 40; objectName: "offButton"; text: qsTr("Turn off"); flat: true
                    onClicked: win.requestAction("off")
                }
                Button {
                    objectName: "saveThemeButton"
                    Layout.fillWidth: true; Layout.preferredHeight: 40
                    enabled: backend.mode === "manual" && backend.activeTheme !== ""
                    text: qsTr("Save to theme: %1").arg(backend.activeTheme || "—")
                    onClicked: backend.saveToTheme(backend.activeTheme)
                }
                Button {
                    objectName: "resetThemeButton"
                    Layout.fillWidth: true; Layout.preferredHeight: 36
                    enabled: backend.activeTheme !== ""
                    text: qsTr("Reset theme: %1").arg(backend.activeTheme || "—")
                    flat: true
                    onClicked: win.requestAction("reset")
                }
                Label {
                    Layout.fillWidth: true; text: backend.notice; visible: text !== ""
                    wrapMode: Text.Wrap; font.pixelSize: 11; color: theme.colors.secondary
                }
                Item { Layout.fillHeight: true }
                Label {
                    Layout.fillWidth: true; text: qsTr("Your choice stays active after closing this window.")
                    wrapMode: Text.Wrap; font.pixelSize: 11; color: theme.colors.subtle
                }
            }
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 20
            RowLayout {
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, (parent.width - credit.implicitWidth) / 2 - 20)
                Rectangle { width: 6; height: 6; radius: 3; color: theme.accent }
                Label { Layout.fillWidth: true; text: backend.status; font.pixelSize: 12; color: theme.colors.secondary; elide: Text.ElideRight }
            }
            Button {
                id: credit
                objectName: "githubLink"
                anchors.centerIn: parent
                text: qsTr("Made by Johannes van Roest Dahl")
                flat: true; padding: 3
                font.pixelSize: 11
                contentItem: Text {
                    text: credit.text; textFormat: Text.PlainText
                    font: credit.font
                    color: credit.hovered || credit.activeFocus ? theme.accent : theme.colors.secondary
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle { color: "transparent"; border.width: credit.activeFocus ? 1 : 0; border.color: theme.accent; radius: theme.radius }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                onClicked: Qt.openUrlExternally("https://github.com/dkingger")
            }
            Button {
                id: shortcuts
                objectName: "shortcutsButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("?  Shortcuts")
                flat: true; padding: 3
                font.pixelSize: 11
                contentItem: Text {
                    text: shortcuts.text; textFormat: Text.PlainText
                    font: shortcuts.font
                    color: shortcuts.hovered || shortcuts.activeFocus ? theme.accent : theme.colors.secondary
                }
                background: Rectangle { color: "transparent"; border.width: shortcuts.activeFocus ? 1 : 0; border.color: theme.accent; radius: theme.radius }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                onClicked: help.open()
            }
        }
    }
}
