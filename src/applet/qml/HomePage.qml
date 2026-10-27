/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

// Common problems, free text and recent conversations
PlasmaComponents3.ScrollView {
    id: page

    signal changesRequested()

    contentWidth: availableWidth

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                text: i18n("What is the problem?")
                wrapMode: Text.WordWrap
            }

            PlasmaComponents3.ToolButton {
                icon.name: "view-history"
                text: i18nc("@action:button", "Changes")
                display: PlasmaComponents3.AbstractButton.IconOnly
                PlasmaComponents3.ToolTip.text: i18n("Changes made by assistant")
                PlasmaComponents3.ToolTip.visible: hovered
                onClicked: page.changesRequested()
            }
        }

        // Quick diagnostic of scenarios still works without language model
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            type: Kirigami.MessageType.Warning
            text: i18n("%1\nScenarios still run a quick diagnostic without assistant. A local model can be installed with Ollama.",
                       Plasmoid.languageModelError)
            visible: Plasmoid.languageModelError.length > 0
            actions: [
                Kirigami.Action {
                    icon.name: "configure"
                    text: i18nc("@action:button", "Configure Language Model…")
                    onTriggered: Plasmoid.showConfigureDialog()
                },
                Kirigami.Action {
                    icon.name: "internet-services"
                    text: i18nc("@action:button", "Ollama Website")
                    onTriggered: Qt.openUrlExternally("https://ollama.com")
                }
            ]
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            columns: 2
            columnSpacing: Kirigami.Units.smallSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            Repeater {
                model: Plasmoid.scenarioModel

                PlasmaComponents3.Button {
                    required property string identifier
                    required property string title
                    required property string iconName
                    required property string description

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    icon.name: iconName
                    text: title
                    Accessible.description: description
                    PlasmaComponents3.ToolTip.text: description
                    PlasmaComponents3.ToolTip.visible: hovered && description.length > 0
                    onClicked: Plasmoid.startScenario(identifier)
                }
            }
        }

        PlasmaComponents3.Switch {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            text: i18n("Allow assistant to change settings (after confirmation)")
            checked: Plasmoid.actionsAllowed
            onToggled: Plasmoid.setActionsAllowed(checked)
        }

        PlasmaComponents3.TextField {
            id: problemField

            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            placeholderText: i18n("Describe your problem…")
            // Keyboard users can type directly when popup is opened
            Component.onCompleted: forceActiveFocus()
            Accessible.name: i18n("Describe your problem")
            onAccepted: {
                Plasmoid.sendMessage(text);
                text = "";
            }
        }

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.topMargin: Kirigami.Units.largeSpacing
            level: 4
            text: i18n("Recent Conversations")
            visible: Plasmoid.recentConversations.length > 0
        }

        Repeater {
            model: Plasmoid.recentConversations

            PlasmaComponents3.ItemDelegate {
                required property var modelData

                Layout.fillWidth: true
                icon.name: "dialog-messages"
                text: modelData.title
                onClicked: Plasmoid.openConversation(modelData.id)
            }
        }
    }
}
