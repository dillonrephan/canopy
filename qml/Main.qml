pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Canopy

ApplicationWindow {
    id: window
    objectName: "canopyWindow"
    property string initialPath: ""
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Canopy"
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
                id: backButton
                objectName: "backButton"
                text: "←"
                enabled: tree.canGoBack
                Accessible.name: "Back"
                ToolTip.visible: hovered
                ToolTip.text: "Back (Alt+Left)"
                onClicked: tree.goBack()

                contentItem: Label {
                    text: backButton.text
                    color: Theme.textDark
                    opacity: backButton.enabled ? 1 : 0.36
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 38
                    implicitHeight: 38
                    radius: 12
                    color: backButton.down ? Theme.selection
                                           : backButton.hovered ? Theme.raisedSurface
                                                                : "transparent"
                    border.color: backButton.hovered ? Theme.border : "transparent"
                }
            }

            ToolButton {
                id: upButton
                objectName: "upButton"
                text: "↑"
                enabled: tree.canGoUp
                Accessible.name: "Parent directory"
                ToolTip.visible: hovered
                ToolTip.text: "Parent directory (Alt+Up)"
                onClicked: tree.goUp()

                contentItem: Label {
                    text: upButton.text
                    color: Theme.textDark
                    opacity: upButton.enabled ? 1 : 0.36
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 38
                    implicitHeight: 38
                    radius: 12
                    color: upButton.down ? Theme.selection
                                         : upButton.hovered ? Theme.raisedSurface
                                                            : "transparent"
                    border.color: upButton.hovered ? Theme.border : "transparent"
                }
            }

            TextField {
                id: addressField
                objectName: "addressField"
                Layout.fillWidth: true
                text: tree.breadcrumb
                selectByMouse: true
                color: Theme.textDark
                font.pixelSize: 14
                Accessible.name: "Directory path"
                onAccepted: {
                    if (tree.navigateToPath(text))
                        focus = false
                }
                onActiveFocusChanged: {
                    if (!activeFocus)
                        text = tree.breadcrumb
                }

                background: Rectangle {
                    radius: 10
                    color: addressField.activeFocus
                           ? Theme.raisedSurface : "transparent"
                    border.color: addressField.activeFocus
                                  ? Theme.selection : "transparent"
                }
            }

            Label {
                text: Math.round(tree.zoom * 100) + "%"
                visible: !tree.perspectiveProjection
                color: Theme.textDark
                opacity: 0.62
                font.pixelSize: 12
            }

            ToolButton {
                id: travelBackButton
                objectName: "travelBackButton"
                text: "−"
                visible: tree.perspectiveProjection
                enabled: tree.canTravelBackward
                Accessible.name: "Travel backward"
                ToolTip.visible: hovered
                ToolTip.text: "Travel backward (Page Up)"
                onClicked: tree.travelBackward()

                background: Rectangle {
                    implicitWidth: 34
                    implicitHeight: 34
                    radius: 11
                    color: travelBackButton.down ? Theme.selection
                        : travelBackButton.hovered ? Theme.raisedSurface
                                                   : "transparent"
                    border.color: travelBackButton.hovered
                                  ? Theme.border : "transparent"
                }
            }

            Label {
                visible: tree.perspectiveProjection
                text: "Depth " + tree.travelDepth.toFixed(1) + " / "
                      + tree.maximumTravelDepth.toFixed(1)
                color: Theme.textDark
                opacity: 0.68
                font.pixelSize: 11
                Accessible.name: text
            }

            ToolButton {
                id: travelForwardButton
                objectName: "travelForwardButton"
                text: "+"
                visible: tree.perspectiveProjection
                enabled: tree.canTravelForward
                Accessible.name: "Travel forward"
                ToolTip.visible: hovered
                ToolTip.text: "Travel forward (Page Down)"
                onClicked: tree.travelForward()

                background: Rectangle {
                    implicitWidth: 34
                    implicitHeight: 34
                    radius: 11
                    color: travelForwardButton.down ? Theme.selection
                        : travelForwardButton.hovered ? Theme.raisedSurface
                                                      : "transparent"
                    border.color: travelForwardButton.hovered
                                  ? Theme.border : "transparent"
                }
            }

            Button {
                id: resetButton
                objectName: "resetButton"
                text: "Reset view"
                enabled: !tree.perspectiveProjection || tree.canResetView
                Accessible.name: "Reset view"
                ToolTip.visible: hovered
                ToolTip.text: "Reset view (Ctrl+0)"
                onClicked: tree.resetView()

                contentItem: Label {
                    text: resetButton.text
                    color: resetButton.down ? Theme.textLight : Theme.textDark
                    opacity: resetButton.enabled ? 1 : 0.36
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 96
                    implicitHeight: 36
                    radius: 12
                    color: resetButton.down ? Theme.selection
                                            : resetButton.hovered ? Theme.raisedSurface
                                                                  : Theme.panelSurface
                    border.color: Theme.border
                }
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
                objectName: "treeScene"
                anchors.fill: parent
                focus: true
                reducedMotion: reducedMotion.checked
                onSelectionChanged: {
                    inspectorTabs.currentIndex = tree.selectedRequiresContents ? 1 : 0
                }
            }

            Connections {
                target: tree

                function onNavigationChanged() {
                    if (!addressField.activeFocus)
                        addressField.text = tree.breadcrumb
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 18
                width: Math.min(parent.width - 36, 620)
                height: instruction.implicitHeight + 16
                radius: 10
                color: Theme.raisedSurface
                border.color: Theme.border

                Label {
                    id: instruction
                    anchors.fill: parent
                    anchors.margins: 8
                    text: tree.perspectiveProjection
                          ? "Scroll to travel  •  Page Up/Down to step  •  double-click space to move  •  double-click branch to enter"
                          : "Scroll to zoom  •  drag to pan  •  click to inspect  •  double-click a branch to enter, or open space to travel"
                    color: Theme.textDark
                    opacity: 0.78
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter
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
                        id: detailsTab
                        text: "Details"

                        contentItem: Label {
                            text: detailsTab.text
                            color: Theme.textDark
                            opacity: detailsTab.checked ? 1 : 0.68
                            font.bold: detailsTab.checked
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 10
                            color: detailsTab.checked ? Theme.raisedSurface
                                                     : detailsTab.hovered ? Theme.shellSurface
                                                                          : "transparent"
                            border.color: detailsTab.checked ? Theme.border
                                                            : "transparent"
                        }
                    }

                    TabButton {
                        id: contentsTab
                        text: "Contents"

                        contentItem: Label {
                            text: contentsTab.text
                            color: Theme.textDark
                            opacity: contentsTab.checked ? 1 : 0.68
                            font.bold: contentsTab.checked
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 10
                            color: contentsTab.checked ? Theme.raisedSurface
                                                      : contentsTab.hovered ? Theme.shellSurface
                                                                            : "transparent"
                            border.color: contentsTab.checked ? Theme.border
                                                             : "transparent"
                        }
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
                                text: tree.contentsModel.rangeLabel
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
                                model: tree.contentsModel
                                ScrollBar.vertical: ScrollBar {}

                                delegate: Rectangle {
                                    id: contentsDelegate
                                    required property string entryName
                                    required property string entryDetail
                                    required property bool entryIsDirectory
                                    required property int index
                                    width: ListView.view.width
                                    height: 42
                                    radius: 5
                                    color: contentsHover.hovered
                                           ? Theme.shellSurface
                                           : index % 2 === 0
                                             ? Theme.raisedSurface : "transparent"
                                    Accessible.name: contentsDelegate.entryName
                                    Accessible.description:
                                        contentsDelegate.entryDetail
                                    Accessible.role: Accessible.ListItem

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 9
                                        anchors.rightMargin: 9
                                        spacing: 8

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0

                                            Label {
                                                Layout.fillWidth: true
                                                text: contentsDelegate.entryName
                                                color: Theme.textDark
                                                elide: Text.ElideMiddle
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: contentsDelegate.entryDetail
                                                color: Theme.textDark
                                                opacity: 0.56
                                                font.pixelSize: 9
                                                elide: Text.ElideMiddle
                                            }
                                        }

                                        Label {
                                            visible: contentsDelegate.entryIsDirectory
                                            text: "branch"
                                            color: Theme.textDark
                                            opacity: 0.52
                                            font.pixelSize: 9
                                        }
                                    }

                                    HoverHandler {
                                        id: contentsHover
                                    }

                                    TapHandler {
                                        acceptedButtons: Qt.LeftButton
                                        onDoubleTapped:
                                            tree.activateContentsRow(
                                                contentsDelegate.index)
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Button {
                                    id: previousPageButton
                                    text: "Previous"
                                    enabled: tree.contentsModel.canPrevious
                                    onClicked: tree.contentsModel.previousPage()

                                    contentItem: Label {
                                        text: previousPageButton.text
                                        color: Theme.textDark
                                        opacity: previousPageButton.enabled ? 1 : 0.42
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        implicitHeight: 34
                                        radius: 10
                                        color: previousPageButton.hovered
                                               ? Theme.raisedSurface
                                               : Theme.shellSurface
                                        border.color: Theme.border
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: tree.contentsModel.loading
                                          ? "Loading page "
                                            + tree.contentsModel.pageNumber + "…"
                                          : "Page " + tree.contentsModel.pageNumber
                                    color: Theme.textDark
                                    opacity: 0.62
                                    horizontalAlignment: Text.AlignHCenter
                                    font.pixelSize: 10
                                }

                                Button {
                                    id: nextPageButton
                                    text: "Next"
                                    enabled: tree.contentsModel.canNext
                                    onClicked: tree.contentsModel.nextPage()

                                    contentItem: Label {
                                        text: nextPageButton.text
                                        color: Theme.textDark
                                        opacity: nextPageButton.enabled ? 1 : 0.42
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        implicitHeight: 34
                                        radius: 10
                                        color: nextPageButton.hovered
                                               ? Theme.raisedSurface
                                               : Theme.shellSurface
                                        border.color: Theme.border
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: tree.selectedRequiresContents
                                text: tree.showHidden
                                      ? "Pages stay memory-bounded. Hidden entries are included."
                                      : "Pages stay memory-bounded. Hidden entries are excluded."
                                color: Theme.textDark
                                opacity: 0.62
                                font.pixelSize: 10
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                CheckBox {
                    id: showHidden
                    text: "Show hidden entries"
                    checked: tree.showHidden
                    onToggled: tree.showHidden = checked

                    indicator: Rectangle {
                        implicitWidth: 20
                        implicitHeight: 20
                        x: showHidden.leftPadding
                        y: (showHidden.height - height) / 2
                        radius: 6
                        color: showHidden.checked ? Theme.selection
                                                  : Theme.raisedSurface
                        border.color: showHidden.hovered ? Theme.selection
                                                         : Theme.border

                        Label {
                            anchors.centerIn: parent
                            visible: showHidden.checked
                            text: "✓"
                            color: Theme.textLight
                            font.bold: true
                            font.pixelSize: 13
                        }
                    }

                    contentItem: Label {
                        text: showHidden.text
                        color: Theme.textDark
                        leftPadding: showHidden.indicator.width
                                     + showHidden.spacing
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                CheckBox {
                    id: reducedMotion
                    objectName: "reducedMotionCheckBox"
                    text: "Reduced motion"
                    checked: false

                    indicator: Rectangle {
                        implicitWidth: 20
                        implicitHeight: 20
                        x: reducedMotion.leftPadding
                        y: (reducedMotion.height - height) / 2
                        radius: 6
                        color: reducedMotion.checked ? Theme.selection
                                                     : Theme.raisedSurface
                        border.color: reducedMotion.hovered ? Theme.selection
                                                            : Theme.border

                        Label {
                            anchors.centerIn: parent
                            visible: reducedMotion.checked
                            text: "✓"
                            color: Theme.textLight
                            font.bold: true
                            font.pixelSize: 13
                        }
                    }

                    contentItem: Label {
                        text: reducedMotion.text
                        color: Theme.textDark
                        leftPadding: reducedMotion.indicator.width
                                     + reducedMotion.spacing
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: tree.pathMessage.length > 0
                    text: tree.pathMessage
                    color: Theme.selection
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    text: "Read-only filesystem view — Canopy never changes files."
                    color: Theme.textDark
                    opacity: 0.58
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+L"
        onActivated: {
            addressField.forceActiveFocus()
            addressField.selectAll()
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

    Shortcut {
        objectName: "pageDownShortcut"
        sequence: "PageDown"
        enabled: !addressField.activeFocus
                 && tree.perspectiveProjection
                 && tree.canTravelForward
        onActivated: tree.travelForward()
    }

    Shortcut {
        objectName: "pageUpShortcut"
        sequence: "PageUp"
        enabled: !addressField.activeFocus
                 && tree.perspectiveProjection
                 && tree.canTravelBackward
        onActivated: tree.travelBackward()
    }

    Shortcut {
        objectName: "resetShortcut"
        sequence: "Ctrl+0"
        enabled: !tree.perspectiveProjection || tree.canResetView
        onActivated: tree.resetView()
    }

    Component.onCompleted: {
        if (initialPath.length > 0)
            tree.navigateToPath(initialPath)
    }
}
