import QtQuick
import qs.Commons as Commons
import qs.Ui

BarWidget {
    id: root
    moduleName: "local.arch-sign"
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight
    Commons.Translations { id: strings; directory: Qt.resolvedUrl("i18n") }
    BarIconButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        tooltipText: strings.t("open")
        iconComponent: Component {
            Image {
                source: Qt.resolvedUrl("arch-sign.svg")
                width: 24
                height: 24
                sourceSize.width: 48
                sourceSize.height: 48
            }
        }
        onPressed: if (root.bar) root.bar.run("omarchy-launch-or-focus arch-sign 'arch-sign open'")
    }
}
