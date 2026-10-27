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
    Plasmoid.status: {
        if (Plasmoid.confirmationPending) {
            return PlasmaCore.Types.RequiresAttentionStatus;
        }
        return Plasmoid.answerInProgress ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus;
    }

    // Changes page is shown on home page
    property bool showChanges: false

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
            sourceComponent: {
                if (Plasmoid.conversationOpen) {
                    return conversationPage;
                }
                return root.showChanges ? changesPage : homePage;
            }
        }

        Component {
            id: homePage
            HomePage {
                onChangesRequested: root.showChanges = true
            }
        }

        Component {
            id: changesPage
            ChangesPage {
                onCloseRequested: root.showChanges = false
            }
        }

        Component {
            id: conversationPage
            ConversationPage {}
        }
    }
}
