/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

// Assistant wants to change system: user allows or denies it.
// Risk "keep": change was applied (display settings), it's reverted unless user keeps it in time
Kirigami.InlineMessage {
    id: card

    required property string requestId
    // Built by daemon from arguments: language model can't present an action under a false light
    required property string summary
    // "pending", "allowed" or "denied"
    required property string status
    // "modify", "destructive" or "keep"
    required property string risk
    // JSON object, contains timeoutSeconds for risk "keep"
    required property string arguments

    readonly property bool pending: status === "pending"
    readonly property bool keepQuestion: risk === "keep"
    // Daemon reverts change after timeout: it's only shown
    property int remainingSeconds: {
        try {
            return JSON.parse(arguments).timeoutSeconds ?? 0;
        } catch (e) {
            return 0;
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: card.keepQuestion && card.pending && card.remainingSeconds > 0
        onTriggered: card.remainingSeconds--
    }

    visible: true
    type: risk === "destructive" ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
    icon.name: "dialog-question"
    text: {
        if (keepQuestion) {
            if (status === "allowed") {
                return i18n("%1 — kept", summary);
            }
            if (status === "denied") {
                return i18n("%1 — reverted", summary);
            }
            return i18np("%2\nPrevious settings are restored in 1 second.", "%2\nPrevious settings are restored in %1 seconds.", remainingSeconds, summary);
        }
        if (status === "allowed") {
            return i18n("%1 — allowed", summary);
        }
        if (status === "denied") {
            return i18n("%1 — denied", summary);
        }
        return risk === "destructive" ? i18n("The assistant wants to: %1\nThis can't be undone.", summary) : i18n("The assistant wants to: %1", summary);
    }
    Accessible.name: i18n("Confirmation")
    Accessible.description: text

    actions: [
        Kirigami.Action {
            visible: card.pending && card.keepQuestion
            icon.name: "dialog-ok-apply"
            text: i18nc("@action:button keep display settings", "Keep")
            onTriggered: Plasmoid.confirm(card.requestId, 1)
        },
        Kirigami.Action {
            visible: card.pending && card.keepQuestion
            icon.name: "edit-undo"
            text: i18nc("@action:button restore previous display settings", "Revert")
            onTriggered: Plasmoid.confirm(card.requestId, 0)
        },
        Kirigami.Action {
            visible: card.pending && !card.keepQuestion
            icon.name: "dialog-ok-apply"
            text: i18nc("@action:button", "Allow")
            onTriggered: Plasmoid.confirm(card.requestId, 1)
        },
        Kirigami.Action {
            // Never for destructive actions
            visible: card.pending && card.risk === "modify"
            icon.name: "dialog-ok"
            text: i18nc("@action:button", "Always Allow")
            tooltip: i18n("Don't ask again for this kind of change")
            onTriggered: Plasmoid.confirm(card.requestId, 2)
        },
        Kirigami.Action {
            visible: card.pending && !card.keepQuestion
            icon.name: "dialog-cancel"
            text: i18nc("@action:button", "Deny")
            onTriggered: Plasmoid.confirm(card.requestId, 0)
        }
    ]
}
