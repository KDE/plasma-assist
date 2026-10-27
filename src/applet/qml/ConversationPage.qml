/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

// Messages of conversation, tool calls and field to answer
ColumnLayout {
    id: page

    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing

        PlasmaComponents3.ToolButton {
            icon.name: "go-previous"
            text: i18nc("@action:button", "Back")
            display: PlasmaComponents3.AbstractButton.IconOnly
            PlasmaComponents3.ToolTip.text: text
            PlasmaComponents3.ToolTip.visible: hovered
            onClicked: Plasmoid.closeConversation()
        }

        Item {
            Layout.fillWidth: true
        }

        PlasmaComponents3.ToolButton {
            icon.name: "media-playback-stop"
            text: i18nc("@action:button", "Stop")
            visible: Plasmoid.answerInProgress
            onClicked: Plasmoid.cancel()
        }
    }

    PlasmaComponents3.ScrollView {
        id: scrollView

        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: listView

            model: Plasmoid.conversationModel
            spacing: Kirigami.Units.largeSpacing
            topMargin: Kirigami.Units.smallSpacing
            bottomMargin: Kirigami.Units.smallSpacing
            clip: true
            reuseItems: false

            // Follow answer while it's written
            onContentHeightChanged: {
                if (Plasmoid.answerInProgress) {
                    positionViewAtEnd();
                }
            }
            onCountChanged: positionViewAtEnd()

            delegate: Item {
                id: delegateItem

                required property string type
                required property string text
                required property string toolName
                required property string status
                required property string arguments
                required property string result
                required property string identifier
                required property string risk

                width: ListView.view.width - Kirigami.Units.largeSpacing * 2
                x: Kirigami.Units.largeSpacing
                implicitHeight: itemLoader.item ? itemLoader.item.implicitHeight : 0

                Loader {
                    id: itemLoader

                    width: parent.width
                    sourceComponent: {
                        switch (delegateItem.type) {
                        case "user":
                            return userMessage;
                        case "tool":
                            return toolCall;
                        case "error":
                            return errorMessage;
                        case "notice":
                            return noticeMessage;
                        case "confirmation":
                            return confirmation;
                        default:
                            return assistantMessage;
                        }
                    }
                }

                Component {
                    id: userMessage

                    Item {
                        implicitHeight: bubble.implicitHeight

                        Rectangle {
                            id: bubble

                            anchors.right: parent.right
                            width: Math.min(parent.width * 0.85, userLabel.implicitWidth + Kirigami.Units.largeSpacing * 2)
                            implicitHeight: userLabel.implicitHeight + Kirigami.Units.largeSpacing * 2
                            radius: Kirigami.Units.cornerRadius
                            color: Qt.alpha(Kirigami.Theme.highlightColor, 0.2)

                            PlasmaComponents3.Label {
                                id: userLabel

                                anchors.fill: parent
                                anchors.margins: Kirigami.Units.largeSpacing
                                text: delegateItem.text
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                Component {
                    id: assistantMessage

                    PlasmaComponents3.Label {
                        text: delegateItem.text
                        textFormat: Text.MarkdownText
                        wrapMode: Text.Wrap
                        Accessible.name: i18n("Answer of assistant")
                        Accessible.description: delegateItem.text
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                }

                Component {
                    id: toolCall

                    ToolCallCard {
                        toolName: delegateItem.toolName
                        status: delegateItem.status
                        arguments: delegateItem.arguments
                        result: delegateItem.result
                    }
                }

                Component {
                    id: noticeMessage

                    Kirigami.InlineMessage {
                        type: Kirigami.MessageType.Information
                        text: delegateItem.text
                        visible: true
                    }
                }

                Component {
                    id: confirmation

                    ConfirmationCard {
                        requestId: delegateItem.identifier
                        summary: delegateItem.text
                        status: delegateItem.status
                        risk: delegateItem.risk
                    }
                }

                Component {
                    id: errorMessage

                    Kirigami.InlineMessage {
                        type: Kirigami.MessageType.Error
                        text: delegateItem.text
                        visible: true
                    }
                }
            }

            footer: PlasmaComponents3.BusyIndicator {
                x: Kirigami.Units.largeSpacing
                running: visible
                visible: Plasmoid.answerInProgress
                height: visible ? implicitHeight : 0
                Accessible.name: i18n("Assistant is answering")
            }
        }
    }

    PlasmaComponents3.TextField {
        id: answerField

        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing
        enabled: !Plasmoid.answerInProgress
        placeholderText: i18n("Answer…")
        onEnabledChanged: {
            if (enabled) {
                forceActiveFocus();
            }
        }
        Accessible.name: i18n("Answer to assistant")
        onAccepted: {
            Plasmoid.sendMessage(text);
            text = "";
        }
    }
}
