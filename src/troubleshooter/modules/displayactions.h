/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/actions.h"
#include <QList>
#include <utility>

class DisplayBackend;
namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
class McpProtocolTool;
}

// Tool set_output_config: change is reverted unless user keeps it
namespace DisplayActions
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, DisplayBackend *backend, ActionRunner *runner);

// Definitions of tools and their actions
[[nodiscard]] QList<std::pair<TextAutoGenerateTextMcpProtocolCore::McpProtocolTool, Action>> actions(DisplayBackend *backend);
}
