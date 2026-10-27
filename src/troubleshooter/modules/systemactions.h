/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/actions.h"

class CommandRunner;
class NetworkBackend;
class PowerBackend;
namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tools set_wifi_enabled, reconnect_network, set_power_profile and restart_plasmashell
namespace SystemActions
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager,
                   NetworkBackend *networkBackend,
                   PowerBackend *powerBackend,
                   CommandRunner *runner,
                   ActionRunner *actionRunner);

[[nodiscard]] Action wifiAction(NetworkBackend *backend);
[[nodiscard]] Action powerProfileAction(PowerBackend *backend);
}
