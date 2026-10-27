/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "backends/powerbackend.h"
#include <QJsonObject>
#include <QStringList>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tools get_power_status and list_sleep_inhibitors
namespace PowerModule
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, PowerBackend *backend);

[[nodiscard]] QJsonObject statusToJson(const PowerStatus &status);
// Only inhibitors which block sleep or screen locking/dimming
[[nodiscard]] QList<SleepInhibitor> blockingInhibitors(const QList<SleepInhibitor> &inhibitors);
[[nodiscard]] QJsonObject inhibitorsToJson(const QList<SleepInhibitor> &inhibitors);
// Obvious problems (low battery, worn battery, automatic sleep disabled…)
[[nodiscard]] QStringList hints(const PowerStatus &status);
}
