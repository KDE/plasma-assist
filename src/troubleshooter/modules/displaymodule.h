/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "backends/displaybackend.h"
#include <QJsonObject>
#include <QStringList>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tool list_outputs
namespace DisplayModule
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, DisplayBackend *backend);

// Connected outputs with details, disconnected outputs by name only
[[nodiscard]] QJsonObject outputsToJson(const QList<DisplayOutput> &outputs);

// Obvious problems (connected but disabled, not in native mode…): helps small language models
[[nodiscard]] QStringList hints(const QList<DisplayOutput> &outputs);
}
