/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "backends/networkbackend.h"
#include <QJsonObject>
#include <QStringList>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tools get_network_status, list_connections and check_connectivity
namespace NetworkModule
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, NetworkBackend *backend);

[[nodiscard]] QJsonObject statusToJson(const NetworkStatus &status);
[[nodiscard]] QJsonObject connectionsToJson(const QList<NetworkConnection> &connections);
// Obvious problems (Wi-Fi disabled, captive portal, no gateway…)
[[nodiscard]] QStringList hints(const NetworkStatus &status);
// Host name accepted by check_connectivity
[[nodiscard]] bool isValidHostName(const QString &host);
}
