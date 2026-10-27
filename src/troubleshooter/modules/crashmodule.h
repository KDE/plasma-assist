/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QJsonObject>

class CommandRunner;
namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tools list_recent_crashes and get_crash_backtrace (systemd-coredump)
namespace CrashModule
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, CommandRunner *runner);

// Output of "coredumpctl --json=short list": crashes (most recent first) and number of crashes by executable
[[nodiscard]] QJsonObject crashesToJson(const QByteArray &output, int limit);
// Output of "coredumpctl info": main fields and stack trace of crashed thread (empty object if format is unknown)
[[nodiscard]] QJsonObject backtraceToJson(const QByteArray &output, int maximumFrames);
[[nodiscard]] QString signalName(int signal);
}
