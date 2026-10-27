/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "networkmanagerbackend.h"
#include <NetworkManagerQt/AccessPoint>
#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/Connection>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/WirelessDevice>
#include <NetworkManagerQt/WirelessSetting>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QHostInfo>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace
{
template<typename Enum>
QString enumName(Enum value)
{
    return QString::fromLatin1(QMetaEnum::fromType<Enum>().valueToKey(value));
}

QString statusName(NetworkManager::Status status)
{
    switch (status) {
    case NetworkManager::Asleep:
        return u"Asleep"_s;
    case NetworkManager::Disconnected:
        return u"Disconnected"_s;
    case NetworkManager::Disconnecting:
        return u"Disconnecting"_s;
    case NetworkManager::Connecting:
        return u"Connecting"_s;
    case NetworkManager::ConnectedLinkLocal:
        return u"ConnectedLinkLocal"_s;
    case NetworkManager::ConnectedSiteOnly:
        return u"ConnectedSiteOnly"_s;
    case NetworkManager::Connected:
        return u"Connected"_s;
    case NetworkManager::Unknown:
        break;
    }
    return u"Unknown"_s;
}

QString connectivityName(NetworkManager::Connectivity connectivity)
{
    switch (connectivity) {
    case NetworkManager::NoConnectivity:
        return u"NoConnectivity"_s;
    case NetworkManager::Portal:
        return u"Portal"_s;
    case NetworkManager::Limited:
        return u"Limited"_s;
    case NetworkManager::Full:
        return u"Full"_s;
    case NetworkManager::UnknownConnectivity:
        break;
    }
    return u"UnknownConnectivity"_s;
}

QStringList addresses(const NetworkManager::IpConfig &config)
{
    QStringList result;
    const NetworkManager::IpAddresses ipAddresses = config.addresses();
    for (const NetworkManager::IpAddress &address : ipAddresses) {
        result.append(u"%1/%2"_s.arg(address.ip().toString()).arg(address.prefixLength()));
    }
    return result;
}

QStringList nameServers(const NetworkManager::IpConfig &config)
{
    QStringList result;
    const QList<QHostAddress> servers = config.nameservers();
    for (const QHostAddress &server : servers) {
        result.append(server.toString());
    }
    return result;
}
}

NetworkManagerBackend::NetworkManagerBackend() = default;

NetworkManagerBackend::~NetworkManagerBackend() = default;

bool NetworkManagerBackend::isAvailable() const
{
    return QDBusConnection::systemBus().interface()->isServiceRegistered(u"org.freedesktop.NetworkManager"_s);
}

NetworkStatus NetworkManagerBackend::status() const
{
    NetworkStatus result;
    result.state = statusName(NetworkManager::status());
    result.connectivity = connectivityName(NetworkManager::connectivity());
    result.networkingEnabled = NetworkManager::isNetworkingEnabled();
    result.wirelessEnabled = NetworkManager::isWirelessEnabled();
    result.wirelessHardwareEnabled = NetworkManager::isWirelessHardwareEnabled();
    if (const NetworkManager::ActiveConnection::Ptr primary = NetworkManager::primaryConnection(); primary) {
        result.primaryConnection = primary->id();
    }
    const NetworkManager::Device::List devices = NetworkManager::networkInterfaces();
    for (const NetworkManager::Device::Ptr &device : devices) {
        // Loopback and virtual interfaces of containers don't help to diagnose
        if (device->type() == NetworkManager::Device::Loopback || device->type() == NetworkManager::Device::Veth
            || device->type() == NetworkManager::Device::Generic) {
            continue;
        }
        NetworkInterface interface;
        interface.name = device->interfaceName();
        interface.type = enumName(device->type());
        interface.state = enumName(device->state());
        if (const NetworkManager::ActiveConnection::Ptr active = device->activeConnection(); active) {
            interface.connection = active->id();
        }
        interface.ipv4Addresses = addresses(device->ipV4Config());
        interface.ipv6Addresses = addresses(device->ipV6Config());
        interface.gateway = device->ipV4Config().gateway();
        interface.nameServers = nameServers(device->ipV4Config()) + nameServers(device->ipV6Config());
        if (const auto wireless = device.objectCast<NetworkManager::WirelessDevice>(); wireless) {
            if (const NetworkManager::AccessPoint::Ptr accessPoint = wireless->activeAccessPoint(); accessPoint) {
                interface.ssid = accessPoint->ssid();
                interface.signalStrength = accessPoint->signalStrength();
            }
        }
        result.interfaces.append(interface);
    }
    return result;
}

QList<NetworkConnection> NetworkManagerBackend::connections() const
{
    QStringList activeConnections;
    const NetworkManager::ActiveConnection::List active = NetworkManager::activeConnections();
    for (const NetworkManager::ActiveConnection::Ptr &connection : active) {
        activeConnections.append(connection->uuid());
    }
    QList<NetworkConnection> result;
    const NetworkManager::Connection::List list = NetworkManager::listConnections();
    for (const NetworkManager::Connection::Ptr &connection : list) {
        // Only settings without secrets are read
        const NetworkManager::ConnectionSettings::Ptr settings = connection->settings();
        const NetworkManager::ConnectionSettings::ConnectionType type = settings->connectionType();
        result.append({
            .name = connection->name(),
            .type = NetworkManager::ConnectionSettings::typeAsString(type),
            .active = activeConnections.contains(connection->uuid()),
            .autoConnect = settings->autoconnect(),
            .vpn = type == NetworkManager::ConnectionSettings::Vpn || type == NetworkManager::ConnectionSettings::WireGuard,
        });
    }
    return result;
}

QStringList NetworkManagerBackend::sensitiveNames() const
{
    QStringList names;
    const NetworkManager::Connection::List list = NetworkManager::listConnections();
    for (const NetworkManager::Connection::Ptr &connection : list) {
        const NetworkManager::ConnectionSettings::Ptr settings = connection->settings();
        // Names of Wi-Fi and VPN connections can locate user, other names are generic (wired connection, bridge…)
        if (const NetworkManager::ConnectionSettings::ConnectionType type = settings->connectionType(); type != NetworkManager::ConnectionSettings::Wireless
            && type != NetworkManager::ConnectionSettings::Vpn && type != NetworkManager::ConnectionSettings::WireGuard) {
            continue;
        }
        names.append(connection->name());
        if (const auto wireless = settings->setting(NetworkManager::Setting::Wireless).staticCast<NetworkManager::WirelessSetting>(); wireless) {
            names.append(QString::fromUtf8(wireless->ssid()));
        }
    }
    return names;
}

void NetworkManagerBackend::checkConnectivity(QObject *context, const ConnectivityCallback &callback)
{
    auto watcher = new QDBusPendingCallWatcher(NetworkManager::checkConnectivity(), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [callback](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<uint> reply = *call;
        if (reply.isError()) {
            callback({}, reply.error().message());
            return;
        }
        callback(connectivityName(static_cast<NetworkManager::Connectivity>(reply.value())), {});
    });
}

void NetworkManagerBackend::resolveHost(const QString &host, QObject *context, const ResolveCallback &callback)
{
    QHostInfo::lookupHost(host, context, [callback](const QHostInfo &info) {
        if (info.error() != QHostInfo::NoError) {
            callback({}, info.errorString());
            return;
        }
        QStringList result;
        const QList<QHostAddress> addresses = info.addresses();
        for (const QHostAddress &address : addresses) {
            result.append(address.toString());
        }
        callback(result, {});
    });
}

void NetworkManagerBackend::checkHttp(QObject *context, const HttpCallback &callback)
{
    if (!mNetworkAccessManager) {
        mNetworkAccessManager = std::make_unique<QNetworkAccessManager>();
    }
    // Server used by NetworkManager of KDE distributions to check connectivity: answers "OK"
    QNetworkRequest request(QUrl(u"http://networkcheck.kde.org/"_s));
    request.setTransferTimeout(10s);
    QNetworkReply *reply = mNetworkAccessManager->get(request);
    // Request is aborted when context is deleted
    QObject::connect(context, &QObject::destroyed, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    QObject::connect(reply, &QNetworkReply::finished, context, [reply, callback]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            callback(statusCode, reply->errorString());
            return;
        }
        callback(statusCode, {});
    });
}

void NetworkManagerBackend::setWirelessEnabled(bool enabled, QObject *context, const ErrorCallback &callback)
{
    Q_UNUSED(context)
    // Property is set asynchronously by NetworkManagerQt
    NetworkManager::setWirelessEnabled(enabled);
    callback({});
}

void NetworkManagerBackend::reconnect(const QString &name, QObject *context, const ErrorCallback &callback)
{
    NetworkManager::Connection::Ptr connection;
    const NetworkManager::Connection::List connections = NetworkManager::listConnections();
    for (const NetworkManager::Connection::Ptr &item : connections) {
        if (item->name() == name) {
            connection = item;
            break;
        }
    }
    if (!connection) {
        callback(u"Unknown connection: %1"_s.arg(name));
        return;
    }
    QString devicePath;
    QString activePath;
    const NetworkManager::ActiveConnection::List active = NetworkManager::activeConnections();
    for (const NetworkManager::ActiveConnection::Ptr &item : active) {
        if (item->uuid() == connection->uuid()) {
            activePath = item->path();
            devicePath = item->devices().value(0);
            break;
        }
    }
    const auto activate = [connectionPath = connection->path(), devicePath, context, callback]() {
        auto watcher = new QDBusPendingCallWatcher(NetworkManager::activateConnection(connectionPath, devicePath, QString()), context);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [callback](QDBusPendingCallWatcher *call) {
            call->deleteLater();
            callback(call->isError() ? call->error().message() : QString());
        });
    };
    if (activePath.isEmpty()) {
        activate();
        return;
    }
    auto watcher = new QDBusPendingCallWatcher(NetworkManager::deactivateConnection(activePath), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [callback, activate](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        if (call->isError()) {
            callback(call->error().message());
            return;
        }
        activate();
    });
}
