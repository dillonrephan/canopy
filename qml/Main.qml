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
    color: Theme.shellSurface

    palette.window: Theme.shellSurface
    palette.windowText: Theme.textDark
    palette.text: Theme.textDark
    palette.button: Theme.raisedSurface
    palette.buttonText: Theme.textDark
    palette.highlight: Theme.selection
    palette.highlightedText: Theme.textLight

    header: ToolBar {
        implicitHeight: 62
        background: Rectangle {
            color: Theme.shellSurface
            border.color: Theme.border
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 12

            Label {
                text: "Canopy"
                font.pixelSize: 22
                font.bold: true
                color: Theme.textDark
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
                color: Theme.textDark
                opacity: 0.76
                elide: Text.ElideMiddle
                font.pixelSize: 14
            }

            Label {
                text: Math.round(tree.zoom * 100) + "%"
                color: Theme.textDark
                opacity: 0.62
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
                onSelectionChanged: {
                    inspectorTabs.currentIndex = tree.selectedRequiresContents ? 1 : 0
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 18
                width: instruction.implicitWidth + 24
                height: instruction.implicitHeight + 16
                radius: 10
                color: Theme.raisedSurface
                border.color: Theme.border

                Label {
                    id: instruction
                    anchors.centerIn: parent
                    text: "Scroll to zoom  •  drag open space to pan  •  click to inspect  •  double-click a branch to focus"
                    color: Theme.textDark
                    opacity: 0.78
                    font.pixelSize: 12
                }
            }
        }

        Pane {
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 260
            padding: 24
            background: Rectangle {
                color: Theme.panelSurface
                border.color: Theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Label {
                    text: "Inspector"
                    color: Theme.textDark
                    opacity: 0.68
                    font.pixelSize: 13
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: tree.selectedName
                    color: Theme.textDark
                    font.pixelSize: 24
                    font.bold: true
                    wrapMode: Text.Wrap
                }

                TabBar {
                    id: inspectorTabs
                    Layout.fillWidth: true
                    visible: tree.selectedIsDirectory

                    TabButton {
                        text: "Details"
                    }

                    TabButton {
                        text: "Contents"
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tree.selectedIsDirectory
                                  ? inspectorTabs.currentIndex : 0

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 1
                                color: Theme.border
                            }

                            Label {
                                text: "Form"
                                color: Theme.textDark
                                opacity: 0.62
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: tree.selectedType
                                color: Theme.textDark
                                wrapMode: Text.Wrap
                            }

                            Label {
                                text: "Path"
                                color: Theme.textDark
                                opacity: 0.62
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: tree.selectedPath
                                color: Theme.textDark
                                wrapMode: Text.WrapAnywhere
                            }

                            Label {
                                text: "Details"
                                color: Theme.textDark
                                opacity: 0.62
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: tree.selectedDetail
                                color: Theme.textDark
                                wrapMode: Text.Wrap
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            Rectangle {
                                Layout.fillWidth: true
                                visible: tree.selectedRequiresContents
                                implicitHeight: contentsStatus.implicitHeight + 24
                                radius: 8
                                color: Theme.noticeSurface
                                border.color: Theme.noticeAccent

                                ColumnLayout {
                                    id: contentsStatus
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 5

                                    Label {
                                        Layout.fillWidth: true
                                        text: tree.selectedDirectoryState
                                        color: Theme.noticeAccent
                                        font.bold: true
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: tree.selectedDirectoryStatus
                                              || "Tree View is showing a bounded sample."
                                        color: Theme.textLight
                                        opacity: 0.84
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: Number(tree.selectedMaterializedChildCount).toLocaleString()
                                      + " of "
                                      + Number(tree.selectedTotalChildCount).toLocaleString()
                                      + " entries available"
                                color: Theme.textDark
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }

                            ListView {
                                id: contentsList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                model: tree.selectedChildNames
                                ScrollBar.vertical: ScrollBar {}

                                delegate: Rectangle {
                                    required property string modelData
                                    required property int index
                                    width: ListView.view.width
                                    height: 34
                                    radius: 5
                                    color: index % 2 === 0
                                           ? Theme.raisedSurface : "transparent"

                                    Label {
                                        anchors.fill: parent
                                        anchors.leftMargin: 9
                                        anchors.rightMargin: 9
                                        text: parent.modelData
                                        color: Theme.textDark
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: tree.selectedRequiresContents
                                text: "The list is a bounded fake sample. Real scanning will load incrementally and remain cancellable."
                                color: Theme.textDark
                                opacity: 0.62
                                font.pixelSize: 10
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                CheckBox {
                    id: reducedMotion
                    text: "Reduced motion"
                    checked: false
                }

                Label {
                    Layout.fillWidth: true
                    text: "Fake filesystem data — no files are being read or changed."
                    color: Theme.textDark
                    opacity: 0.58
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
