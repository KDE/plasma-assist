/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

// Tool called by assistant: folded by default, unfold to see arguments and result
ColumnLayout {
    id: card

    required property string toolName
    // "running", "done" or "failed"
    required property string status
    required property string arguments
    required property string result

    property bool expanded: false

    spacing: Kirigami.Units.smallSpacing

    PlasmaComponents3.ItemDelegate {
        Layout.fillWidth: true
        icon.name: card.expanded ? "arrow-down" : "arrow-right"
        text: {
            switch (card.status) {
            case "running":
                return i18nc("@info tool is running", "Running %1…", card.toolName);
            case "failed":
                return i18nc("@info tool failed", "%1 failed", card.toolName);
            default:
                return card.toolName;
            }
        }
        Accessible.name: text
        Accessible.description: card.expanded ? i18n("Hide details of tool call") : i18n("Show details of tool call")
        onClicked: card.expanded = !card.expanded

        Kirigami.Icon {
            anchors.right: parent.right
            anchors.rightMargin: Kirigami.Units.smallSpacing
            anchors.verticalCenter: parent.verticalCenter
            width: Kirigami.Units.iconSizes.small
            height: width
            source: card.status === "failed" ? "dialog-error" : (card.status === "done" ? "dialog-ok" : "")
            visible: card.status !== "running"
        }
    }

    PlasmaComponents3.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        visible: card.expanded
        text: i18n("Arguments: %1", card.arguments.length > 0 ? card.arguments : "{}")
        font.family: "monospace"
        wrapMode: Text.WrapAnywhere
        textFormat: Text.PlainText
    }

    PlasmaComponents3.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        visible: card.expanded && card.result.length > 0
        text: card.result
        font.family: "monospace"
        wrapMode: Text.WrapAnywhere
        textFormat: Text.PlainText
        maximumLineCount: 40
        elide: Text.ElideRight
    }
}
