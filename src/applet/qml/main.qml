/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root

    Plasmoid.icon: "help-browser"
    // Hidden in system tray until a conversation is in progress
    Plasmoid.status: Plasmoid.answerInProgress ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    toolTipMainText: i18n("Plasma Assist")
    toolTipSubText: Plasmoid.answerInProgress ? i18n("Assistant is answering…") : i18n("Diagnose and fix common problems")

    onExpandedChanged: {
        if (expanded && !Plasmoid.conversationOpen) {
            Plasmoid.refreshConversations();
        }
    }

    fullRepresentation: PlasmaExtras.Representation {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 18
        Layout.minimumHeight: Kirigami.Units.gridUnit * 14
        Layout.preferredWidth: Kirigami.Units.gridUnit * 24
        Layout.preferredHeight: Kirigami.Units.gridUnit * 28

        collapseMarginsHint: true

        Loader {
            anchors.fill: parent
            sourceComponent: Plasmoid.conversationOpen ? conversationPage : homePage
        }

        Component {
            id: homePage
            HomePage {}
        }

        Component {
            id: conversationPage
            ConversationPage {}
        }
    }
}
