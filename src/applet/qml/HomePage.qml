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

    contentWidth: availableWidth

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            level: 2
            text: i18n("What is the problem?")
            wrapMode: Text.WordWrap
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            type: Kirigami.MessageType.Warning
            text: Plasmoid.languageModelError
            visible: text.length > 0
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
                    required property string title
                    required property string iconName
                    required property string prompt

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    icon.name: iconName
                    text: title
                    Accessible.description: prompt
                    onClicked: Plasmoid.sendMessage(prompt)
                }
            }
        }

        PlasmaComponents3.TextField {
            id: problemField

            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            placeholderText: i18n("Describe your problem…")
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
