import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Canopy

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Canopy — Phase 1 Prototype"
    color: "#101713"

    palette.window: "#101713"
    palette.windowText: "#e6eadf"
    palette.text: "#e6eadf"
    palette.button: "#26352b"
    palette.buttonText: "#e6eadf"
    palette.highlight: "#477a55"

    header: ToolBar {
        implicitHeight: 58
        background: Rectangle {
            color: "#172019"
            border.color: "#2d3b30"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 12

            Label {
                text: "CANOPY"
                font.pixelSize: 17
                font.bold: true
                font.letterSpacing: 2.5
                color: "#c9d8bd"
            }

            ToolSeparator {}

            ToolButton {
                text: "←"
                enabled: tree.canGoBack
                Accessible.name: "Back"
                ToolTip.visible: hovered
                ToolTip.text: "Back (Alt+Left)"
                onClicked: tree.goBack()
            }

            ToolButton {
                text: "↑"
                enabled: tree.canGoUp
                Accessible.name: "Parent directory"
                ToolTip.visible: hovered
                ToolTip.text: "Parent directory (Alt+Up)"
                onClicked: tree.goUp()
            }

            Label {
                Layout.fillWidth: true
                text: tree.breadcrumb
                color: "#b8c3b3"
                elide: Text.ElideMiddle
                font.pixelSize: 14
            }

            Label {
                text: Math.round(tree.zoom * 100) + "%"
                color: "#8fa08d"
                font.pixelSize: 12
            }

            Button {
                text: "Reset view"
                onClicked: tree.resetView()
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Item {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 600

            TreeScene {
                id: tree
                anchors.fill: parent
                reducedMotion: reducedMotion.checked
            }

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 18
                width: instruction.implicitWidth + 24
                height: instruction.implicitHeight + 16
                radius: 10
                color: "#c8172019"
                border.color: "#38483b"

                Label {
                    id: instruction
                    anchors.centerIn: parent
                    text: "Scroll to zoom  •  drag open space to pan  •  click to inspect  •  double-click a branch to focus"
                    color: "#aebaaa"
                    font.pixelSize: 12
                }
            }
        }

        Pane {
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 260
            padding: 24
            background: Rectangle {
                color: "#19231c"
                border.color: "#314035"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 16

                Label {
                    text: "INSPECTOR"
                    color: "#839480"
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1.8
                }

                Label {
                    Layout.fillWidth: true
                    text: tree.selectedName
                    color: "#edf0e8"
                    font.pixelSize: 24
                    font.bold: true
                    wrapMode: Text.Wrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: "#354338"
                }

                Label {
                    text: "FORM"
                    color: "#71806f"
                    font.pixelSize: 10
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: tree.selectedType
                    color: "#c5d0c1"
                    wrapMode: Text.Wrap
                }

                Label {
                    text: "PATH"
                    color: "#71806f"
                    font.pixelSize: 10
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: tree.selectedPath
                    color: "#c5d0c1"
                    wrapMode: Text.WrapAnywhere
                }

                Label {
                    text: "DETAILS"
                    color: "#71806f"
                    font.pixelSize: 10
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: tree.selectedDetail
                    color: "#c5d0c1"
                    wrapMode: Text.Wrap
                }

                Item { Layout.fillHeight: true }

                CheckBox {
                    id: reducedMotion
                    text: "Reduced motion"
                    checked: false
                }

                Label {
                    Layout.fillWidth: true
                    text: "Fake filesystem data — no files are being read or changed."
                    color: "#71806f"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Shortcut {
        sequence: "Alt+Left"
        enabled: tree.canGoBack
        onActivated: tree.goBack()
    }

    Shortcut {
        sequence: "Alt+Up"
        enabled: tree.canGoUp
        onActivated: tree.goUp()
    }
}
