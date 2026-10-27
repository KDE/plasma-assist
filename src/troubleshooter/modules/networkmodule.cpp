/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "networkmodule.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QJsonArray>
#include <QPointer>
#include <QRegularExpression>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr int weakSignalStrength = 30;
const QString defaultHost = u"kde.org"_s;

bool isActivated(const NetworkInterface &interface)
{
    return interface.state == "Activated"_L1;
}

// Result of check_connectivity, sent when the three checks are finished
struct ConnectivityCheck {
    QJsonObject result{
        {u"networkManager"_s, QJsonObject{}},
        {u"dns"_s, QJsonObject{}},
        {u"http"_s, QJsonObject{}},
    };
    int pending = 3;
};
}

QJsonObject NetworkModule::statusToJson(const NetworkStatus &status)
{
    QJsonArray interfaces;
    for (const NetworkInterface &interface : status.interfaces) {
        QJsonObject object{
            {u"name"_s, interface.name},
            {u"type"_s, interface.type},
            {u"state"_s, interface.state},
            {u"connection"_s, interface.connection},
            {u"ipv4Addresses"_s, QJsonArray::fromStringList(interface.ipv4Addresses)},
            {u"ipv6Addresses"_s, QJsonArray::fromStringList(interface.ipv6Addresses)},
            {u"gateway"_s, interface.gateway},
            {u"nameServers"_s, QJsonArray::fromStringList(interface.nameServers)},
        };
        if (!interface.ssid.isEmpty()) {
            object.insert(u"ssid"_s, interface.ssid);
            object.insert(u"signalStrength"_s, interface.signalStrength);
        }
        interfaces.append(object);
    }
    return QJsonObject{
        {u"state"_s, status.state},
        {u"connectivity"_s, status.connectivity},
        {u"networkingEnabled"_s, status.networkingEnabled},
        {u"wirelessEnabled"_s, status.wirelessEnabled},
        {u"wirelessHardwareEnabled"_s, status.wirelessHardwareEnabled},
        {u"primaryConnection"_s, status.primaryConnection},
        {u"interfaces"_s, interfaces},
        {u"hints"_s, QJsonArray::fromStringList(hints(status))},
    };
}

QJsonObject NetworkModule::connectionsToJson(const QList<NetworkConnection> &connections)
{
    QJsonArray array;
    for (const NetworkConnection &connection : connections) {
        array.append(QJsonObject{
            {u"name"_s, connection.name},
            {u"type"_s, connection.type},
            {u"active"_s, connection.active},
            {u"autoConnect"_s, connection.autoConnect},
            {u"vpn"_s, connection.vpn},
        });
    }
    return QJsonObject{{u"connections"_s, array}};
}

QStringList NetworkModule::hints(const NetworkStatus &status)
{
    QStringList result;
    if (!status.networkingEnabled) {
        result.append(u"Networking is disabled in NetworkManager"_s);
        return result;
    }
    const bool hasWifi = std::ranges::any_of(status.interfaces, [](const NetworkInterface &interface) {
        return interface.type == "Wifi"_L1;
    });
    if (hasWifi && !status.wirelessHardwareEnabled) {
        result.append(u"Wi-Fi is disabled by a hardware switch or a key (airplane mode)"_s);
    } else if (hasWifi && !status.wirelessEnabled) {
        result.append(u"Wi-Fi is disabled"_s);
    }
    if (status.connectivity == "Portal"_L1) {
        result.append(u"Network requires a login in a web page (captive portal): open a web browser"_s);
    } else if (status.connectivity == "Limited"_L1) {
        result.append(u"Connected to a network, but Internet can't be reached"_s);
    }
    QList<NetworkInterface> activated;
    std::ranges::copy_if(status.interfaces, std::back_inserter(activated), isActivated);
    if (activated.isEmpty()) {
        result.append(u"No active network connection"_s);
    }
    for (const NetworkInterface &interface : activated) {
        if (interface.ipv4Addresses.isEmpty() && interface.ipv6Addresses.isEmpty()) {
            result.append(u"%1 has no IP address"_s.arg(interface.name));
        }
        // Interfaces without gateway (bridges of virtual machines…) don't need DNS
        if (interface.nameServers.isEmpty() && !interface.gateway.isEmpty()) {
            result.append(u"%1 has no DNS server"_s.arg(interface.name));
        }
        if (interface.signalStrength >= 0 && interface.signalStrength < weakSignalStrength) {
            result.append(u"Wi-Fi signal of %1 is weak (%2%)"_s.arg(interface.name).arg(interface.signalStrength));
        }
    }
    const bool hasGateway = std::ranges::any_of(activated, [](const NetworkInterface &interface) {
        return !interface.gateway.isEmpty();
    });
    if (!activated.isEmpty() && !hasGateway) {
        result.append(u"No default gateway: only local network can be reached"_s);
    }
    return result;
}

bool NetworkModule::isValidHostName(const QString &host)
{
    static const QRegularExpression expression(
        u"^(?=.{1,253}$)[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?(?:\\.[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)*$"_s);
    return expression.match(host).hasMatch();
}

void NetworkModule::registerTools(McpProtocolServerProtocolManager *manager, NetworkBackend *backend)
{
    const ToolHelpers::ToolGroup group{u"network"_s, i18nc("@title tools group", "Network")};
    const auto checkAvailable = [backend](McpProtocolServerToolCall *call) {
        if (!backend->isAvailable()) {
            ToolHelpers::finishWithError(call, u"NetworkManager is not running"_s);
            return false;
        }
        return true;
    };

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"get_network_status"_s,
                                                         i18nc("@title tool", "Network Status"),
                                                         u"Get network state: connectivity (full, captive portal, limited), Wi-Fi enabled, interfaces with "
                                                         "their state, addresses, gateway, DNS servers and Wi-Fi signal. Result contains hints about obvious "
                                                         "problems. Use it first when Internet or Wi-Fi doesn't work."_s,
                                                         ToolHelpers::ToolKind::Diagnostic,
                                                         group),
                                 [backend, checkAvailable](McpProtocolServerToolCall *call) {
                                     if (!checkAvailable(call)) {
                                         return;
                                     }
                                     const NetworkStatus status = backend->status();
                                     QString summary = u"Network %1, connectivity %2"_s.arg(status.state, status.connectivity);
                                     if (const QStringList problems = hints(status); !problems.isEmpty()) {
                                         summary += u". "_s + problems.join(u". "_s);
                                     }
                                     ToolHelpers::finishWithJson(call, statusToJson(status), summary);
                                 });

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"list_connections"_s,
                                                         i18nc("@title tool", "Network Connections"),
                                                         u"List saved network connections (Wi-Fi, wired, VPN) with their type, and whether they are active "
                                                         "or connected automatically. Passwords and keys are never returned."_s,
                                                         ToolHelpers::ToolKind::Diagnostic,
                                                         group),
                                 [backend, checkAvailable](McpProtocolServerToolCall *call) {
                                     if (!checkAvailable(call)) {
                                         return;
                                     }
                                     const QList<NetworkConnection> connections = backend->connections();
                                     ToolHelpers::finishWithJson(call, connectionsToJson(connections), u"%1 saved connections"_s.arg(connections.count()));
                                 });

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(
            u"check_connectivity"_s,
            i18nc("@title tool", "Check Connectivity"),
            u"Check Internet access now: connectivity check of NetworkManager, DNS resolution of a host and HTTP request to "
            "networkcheck.kde.org. Use it to know whether DNS or Internet access fails."_s,
            ToolHelpers::ToolKind::Diagnostic,
            group,
            {{u"host"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Host name to resolve (default: %1)"_s.arg(defaultHost)}}}},
            {},
            true),
        [backend, checkAvailable](McpProtocolServerToolCall *call) {
            const QString host = call->arguments().value(u"host"_s).toString(defaultHost);
            if (!isValidHostName(host)) {
                ToolHelpers::finishWithError(call, u"Invalid host name: %1"_s.arg(host));
                return;
            }
            if (!checkAvailable(call)) {
                return;
            }
            auto check = std::make_shared<ConnectivityCheck>();
            const auto done = [call, check](const QString &key, const QJsonObject &value) {
                check->result.insert(key, value);
                if (--check->pending > 0) {
                    return;
                }
                const QString connectivity = check->result.value("networkManager"_L1).toObject().value("connectivity"_L1).toString();
                const bool dns = check->result.value("dns"_L1).toObject().value("ok"_L1).toBool();
                const bool http = check->result.value("http"_L1).toObject().value("ok"_L1).toBool();
                ToolHelpers::finishWithJson(call,
                                            check->result,
                                            u"Connectivity: %1, DNS: %2, HTTP: %3"_s.arg(connectivity.isEmpty() ? u"unknown"_s : connectivity,
                                                                                         dns ? u"ok"_s : u"failed"_s,
                                                                                         http ? u"ok"_s : u"failed"_s));
            };
            backend->checkConnectivity(call, [done](const QString &connectivity, const QString &error) {
                done(u"networkManager"_s, QJsonObject{{u"connectivity"_s, connectivity}, {u"error"_s, error}});
            });
            backend->resolveHost(host, call, [done, host](const QStringList &addresses, const QString &error) {
                done(u"dns"_s,
                     QJsonObject{{u"host"_s, host}, {u"ok"_s, error.isEmpty()}, {u"addresses"_s, QJsonArray::fromStringList(addresses)}, {u"error"_s, error}});
            });
            backend->checkHttp(call, [done](int statusCode, const QString &error) {
                done(u"http"_s, QJsonObject{{u"ok"_s, error.isEmpty() && statusCode == 200}, {u"statusCode"_s, statusCode}, {u"error"_s, error}});
            });
        });
}
