/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tool get_system_report and resource system://report: Markdown report made with diagnostic tools,
// to attach to a bug report
namespace ReportModule
{
// Must be called after registration of tools used by report
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager);

struct Section {
    QString title;
    // MCP result of tool (content, structuredContent, isError)
    QJsonObject result;
};
[[nodiscard]] QString reportToMarkdown(const QList<Section> &sections, const QString &dateTime);
}
