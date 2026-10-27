/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

class QObject;

struct NetworkInterface {
    QString name;
    QString type; // Ethernet, Wifi, Bluetooth, Modem…
    QString state; // Activated, Disconnected, Unavailable (cable unplugged, Wi-Fi off)…
    QString connection; // active connection
    QStringList ipv4Addresses;
    QStringList ipv6Addresses;
    QString gateway;
    QStringList nameServers;
    QString ssid; // Wi-Fi only
    int signalStrength = -1; // Wi-Fi only, percent
};

struct NetworkStatus {
    QString state; // Connected, Disconnected, ConnectedSiteOnly…
    QString connectivity; // Full, Portal, Limited, NoConnectivity, UnknownConnectivity
    bool networkingEnabled = true;
    bool wirelessEnabled = true;
    bool wirelessHardwareEnabled = true; // hardware switch
    QString primaryConnection;
    QList<NetworkInterface> interfaces;
};

struct NetworkConnection {
    QString name;
    QString type; // 802-3-ethernet, 802-11-wireless, vpn, wireguard…
    bool active = false;
    bool autoConnect = true;
    bool vpn = false;
};

// Network state (secrets are never read)
class NetworkBackend
{
public:
    using ConnectivityCallback = std::function<void(const QString &connectivity, const QString &error)>;
    using ResolveCallback = std::function<void(const QStringList &addresses, const QString &error)>;
    using HttpCallback = std::function<void(int statusCode, const QString &error)>;
    using ErrorCallback = std::function<void(const QString &error)>;

    NetworkBackend();
    virtual ~NetworkBackend();

    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual NetworkStatus status() const = 0;
    [[nodiscard]] virtual QList<NetworkConnection> connections() const = 0;
    // Names which identify user (Wi-Fi networks): masked by PrivacyFilter
    [[nodiscard]] virtual QStringList sensitiveNames() const = 0;

    // Asks NetworkManager to check connectivity now
    virtual void checkConnectivity(QObject *context, const ConnectivityCallback &callback) = 0;
    virtual void resolveHost(const QString &host, QObject *context, const ResolveCallback &callback) = 0;
    // HTTP request to connectivity check server of KDE
    virtual void checkHttp(QObject *context, const HttpCallback &callback) = 0;

    virtual void setWirelessEnabled(bool enabled, QObject *context, const ErrorCallback &callback) = 0;
    // Deactivates connection \a name if it's active, then activates it
    virtual void reconnect(const QString &name, QObject *context, const ErrorCallback &callback) = 0;

private:
    Q_DISABLE_COPY_MOVE(NetworkBackend)
};
