/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <Plasma/Applet>

class OrgKdePlasmaAssistAssistantInterface;
class QDBusServiceWatcher;

// Applet: user interface of plasma-assistd (D-Bus)
class AssistApplet : public Plasma::Applet
{
    Q_OBJECT
    Q_PROPERTY(bool daemonAvailable READ daemonAvailable NOTIFY daemonAvailableChanged)
    Q_PROPERTY(QString daemonStatus READ daemonStatus NOTIFY daemonStatusChanged)
public:
    AssistApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~AssistApplet() override;

    [[nodiscard]] bool daemonAvailable() const;
    // JSON status sent by daemon
    [[nodiscard]] QString daemonStatus() const;

    // Starts daemon by D-Bus activation
    Q_INVOKABLE void startDaemon();

Q_SIGNALS:
    void daemonAvailableChanged();
    void daemonStatusChanged();

private:
    void setDaemonAvailable(bool available);
    void setDaemonStatus(const QString &status);
    void fetchStatus();

    OrgKdePlasmaAssistAssistantInterface *const mInterface;
    QDBusServiceWatcher *const mServiceWatcher;
    QString mDaemonStatus;
    bool mDaemonAvailable = false;
};
