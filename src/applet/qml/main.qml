/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root

    Plasmoid.icon: "help-browser"
    Plasmoid.status: PlasmaCore.Types.PassiveStatus

    toolTipMainText: i18n("Plasma Assist")
    toolTipSubText: i18n("Diagnose and fix common problems")

    fullRepresentation: PlasmaExtras.Representation {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 18
        Layout.minimumHeight: Kirigami.Units.gridUnit * 14
        Layout.preferredWidth: Kirigami.Units.gridUnit * 24
        Layout.preferredHeight: Kirigami.Units.gridUnit * 28

        collapseMarginsHint: true

        PlasmaExtras.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4

            iconName: Plasmoid.daemonAvailable ? "help-browser" : "dialog-warning"
            text: Plasmoid.daemonAvailable ? i18n("Plasma Assist") : i18n("Assistant service is not running")
            // TODO: show scenarios and conversation
            explanation: Plasmoid.daemonAvailable ? i18n("Not implemented yet.") : ""

            helpfulAction: Kirigami.Action {
                enabled: !Plasmoid.daemonAvailable
                visible: enabled
                icon.name: "system-run"
                text: i18n("Start Service")
                onTriggered: Plasmoid.startDaemon()
            }
        }
    }
}
