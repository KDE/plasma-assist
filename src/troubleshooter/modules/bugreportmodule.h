/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QJsonObject>
#include <QString>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tool prepare_bug_report: Markdown report (description, crash, backtrace, system, journal) to paste on bugs.kde.org
namespace BugReportModule
{
// Must be called after registration of tools used by report
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager);

struct ReportData {
    QString description;
    // structuredContent of get_crash_backtrace, get_system_info and read_journal (empty when not available)
    QJsonObject crash;
    QJsonObject system;
    QJsonObject journal;
};
[[nodiscard]] QString reportToMarkdown(const ReportData &data);
}
