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

// Changes made by assistant, which can be undone
ColumnLayout {
    id: page

    signal closeRequested()

    spacing: 0

    Component.onCompleted: Plasmoid.refreshChanges()

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing

        PlasmaComponents3.ToolButton {
            icon.name: "go-previous"
            text: i18nc("@action:button", "Back")
            display: PlasmaComponents3.AbstractButton.IconOnly
            PlasmaComponents3.ToolTip.text: text
            PlasmaComponents3.ToolTip.visible: hovered
            onClicked: page.closeRequested()
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 3
            text: i18n("Changes Made by Assistant")
            elide: Text.ElideRight
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing
        type: Kirigami.MessageType.Error
        text: Plasmoid.changesError
        visible: text.length > 0
    }

    PlasmaComponents3.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: listView

            model: Plasmoid.changes
            clip: true

            delegate: PlasmaComponents3.ItemDelegate {
                id: delegate

                required property var modelData

                width: ListView.view.width
                Accessible.name: modelData.summary

                contentItem: RowLayout {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            text: delegate.modelData.summary
                            wrapMode: Text.Wrap
                            font.strikeout: delegate.modelData.undone
                        }
                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            text: delegate.modelData.undone ? i18n("%1 — undone", Qt.formatDateTime(new Date(delegate.modelData.dateTime)))
                                                            : Qt.formatDateTime(new Date(delegate.modelData.dateTime))
                            font: Kirigami.Theme.smallFont
                            opacity: 0.7
                        }
                    }

                    PlasmaComponents3.Button {
                        visible: delegate.modelData.undoable
                        icon.name: "edit-undo"
                        text: i18nc("@action:button", "Undo")
                        Accessible.description: i18n("Undo: %1", delegate.modelData.summary)
                        onClicked: Plasmoid.undoChange(delegate.modelData.id)
                    }
                }
            }

            PlasmaExtras.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: listView.count === 0
                iconName: "edit-undo"
                text: i18n("No change was made")
            }
        }
    }
}
