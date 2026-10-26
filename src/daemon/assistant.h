/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDBusContext>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>

class TroubleshooterClient;

// Object exported on D-Bus (org.kde.plasma.assist.Assistant)
class Assistant : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    explicit Assistant(QObject *parent = nullptr);
    ~Assistant() override;

    // Exports object and requests service name on session bus
    [[nodiscard]] bool registerOnSessionBus();

    [[nodiscard]] TroubleshooterClient *troubleshooterClient() const;

    // Calls tool without language model. \a callback receives JSON result ("isError" is true on error)
    void callTool(const QString &name, const QString &argumentsJson, QObject *context, const std::function<void(const QString &result)> &callback);

    // D-Bus API
    [[nodiscard]] QString Status() const;
    [[nodiscard]] QString NewConversation();
    [[nodiscard]] QString ListTools();
    QString CallTool(const QString &name, const QString &argumentsJson);

Q_SIGNALS:
    void StatusChanged(const QString &status);
    // Daemon is not used anymore: application can quit
    void idleTimeout();

private:
    void restartIdleTimer();
    void slotIdleTimeout();

    TroubleshooterClient *const mTroubleshooterClient;
    QTimer mIdleTimer;
    QStringList mConversations;
};
