/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

class CommandRunner;
namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tool read_journal (journald)
namespace LogModule
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, CommandRunner *runner);

// Arguments of journalctl for tool arguments, or error when an argument is invalid
struct JournalArguments {
    QStringList arguments;
    QString error;
    int limit = 0;
};
[[nodiscard]] JournalArguments journalArguments(const QMap<QString, QJsonValue> &toolArguments);
// Output of "journalctl -o json": one line by entry
[[nodiscard]] QJsonObject entriesToJson(const QByteArray &output);
}
