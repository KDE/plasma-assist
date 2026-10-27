/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "networkbackend.h"

class QNetworkAccessManager;

// NetworkBackend implemented with NetworkManagerQt
class NetworkManagerBackend : public NetworkBackend
{
public:
    NetworkManagerBackend();
    ~NetworkManagerBackend() override;

    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] NetworkStatus status() const override;
    [[nodiscard]] QList<NetworkConnection> connections() const override;
    [[nodiscard]] QStringList sensitiveNames() const override;

    void checkConnectivity(QObject *context, const ConnectivityCallback &callback) override;
    void resolveHost(const QString &host, QObject *context, const ResolveCallback &callback) override;
    void checkHttp(QObject *context, const HttpCallback &callback) override;
    void setWirelessEnabled(bool enabled, QObject *context, const ErrorCallback &callback) override;
    void reconnect(const QString &name, QObject *context, const ErrorCallback &callback) override;

private:
    std::unique_ptr<QNetworkAccessManager> mNetworkAccessManager;
};
