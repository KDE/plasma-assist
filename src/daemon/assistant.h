/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QStringList>

// Object exported on D-Bus (org.kde.plasma.assist.Assistant)
class Assistant : public QObject
{
    Q_OBJECT
public:
    explicit Assistant(QObject *parent = nullptr);
    ~Assistant() override;

    // Exports object and requests service name on session bus
    [[nodiscard]] bool registerOnSessionBus();

    [[nodiscard]] QString Status() const;
    [[nodiscard]] QString NewConversation();

Q_SIGNALS:
    void StatusChanged(const QString &status);

private:
    QStringList mConversations;
};
