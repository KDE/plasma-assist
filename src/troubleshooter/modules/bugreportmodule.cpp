/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bugreportmodule.h"
#include "core/privacyfilter.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr int journalEntries = 30;
// Journal is read before crash
constexpr int journalMinutesBeforeCrash = 10;

QJsonObject structuredContent(const QJsonObject &result)
{
    return result.value("isError"_L1).toBool() ? QJsonObject() : result.value("structuredContent"_L1).toObject();
}

// Data are collected one after another: journal needs executable of crash
struct ReportRequest {
    BugReportModule::ReportData data;
    qint64 pid = -1;
};
}

QString BugReportModule::reportToMarkdown(const ReportData &data)
{
    QString report = u"# Bug Report\n\n## Description\n\n"_s;
    report += data.description.trimmed().isEmpty() ? u"(Describe what you were doing when the problem happened, and how to reproduce it.)\n"_s
                                                   : data.description.trimmed() + u'\n';

    if (!data.crash.isEmpty()) {
        report += u"\n## Crash\n\n- Application: %1\n- Signal: %2\n- Date: %3\n"_s.arg(data.crash.value("executable"_L1).toString(),
                                                                                       data.crash.value("signal"_L1).toString(),
                                                                                       data.crash.value("dateTime"_L1).toString());
        const QJsonArray frames = data.crash.value("frames"_L1).toArray();
        if (frames.isEmpty()) {
            report += u"\nNo backtrace is available.\n"_s;
        } else {
            report += u"\n### Backtrace of crashed thread\n\n```\n"_s;
            for (const QJsonValue &frame : frames) {
                report += frame.toString() + u'\n';
            }
            report += u"```\n"_s;
        }
    }

    if (!data.system.isEmpty()) {
        const QJsonObject distribution = data.system.value("distribution"_L1).toObject();
        QStringList drivers;
        const QJsonArray cards = data.system.value("graphicCards"_L1).toArray();
        for (const QJsonValue &card : cards) {
            drivers.append(card.toObject().value("driver"_L1).toString());
        }
        report += u"\n## System\n\n- Distribution: %1\n- Kernel: %2\n- KDE Frameworks: %3\n- Qt: %4\n- Session: %5\n- Graphic driver: %6\n"_s.arg(
            distribution.value("PRETTY_NAME"_L1).toString(),
            data.system.value("kernel"_L1).toObject().value("version"_L1).toString(),
            data.system.value("frameworksVersion"_L1).toString(),
            data.system.value("qtVersion"_L1).toString(),
            data.system.value("session"_L1).toObject().value("type"_L1).toString(),
            drivers.join(u", "_s));
    }

    if (const QJsonArray entries = data.journal.value("entries"_L1).toArray(); !entries.isEmpty()) {
        report += u"\n## Journal\n\n```\n"_s;
        for (const QJsonValue &value : entries) {
            const QJsonObject entry = value.toObject();
            report +=
                u"%1 %2: %3\n"_s.arg(entry.value("dateTime"_L1).toString(), entry.value("identifier"_L1).toString(), entry.value("message"_L1).toString());
        }
        report += u"```\n"_s;
    }

    report +=
        u"\n---\nReport it on https://bugs.kde.org (choose the product of the application). A backtrace with debug symbols "
        "(installed with the debug packages of your distribution, or given by the crash handler DrKonqi) is more useful. "
        "Personal data are masked.\n"_s;
    return report;
}

void BugReportModule::registerTools(McpProtocolServerProtocolManager *manager)
{
    const ToolHelpers::ToolGroup group{u"crashes"_s, i18nc("@title tools group", "Crashes")};

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(
            u"prepare_bug_report"_s,
            i18nc("@title tool", "Prepare Bug Report"),
            u"Prepare a Markdown bug report for bugs.kde.org: description, crash with backtrace (when pid of a crash is given), "
            "system information and journal before crash. Personal data are masked. Give the report to the user as is."_s,
            ToolHelpers::ToolKind::Diagnostic,
            group,
            {
                {u"pid"_s, QJsonObject{{u"type"_s, u"integer"_s}, {u"description"_s, u"Process id of a crash given by list_recent_crashes"_s}}},
                {u"description"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Description of the problem by the user"_s}}},
            }),
        [manager](McpProtocolServerToolCall *call) {
            auto request = std::make_shared<ReportRequest>();
            request->data.description = PrivacyFilter::instance()->restore(call->arguments().value(u"description"_s)).toString();
            request->pid = ToolHelpers::integerArgument(call->arguments().value(u"pid"_s)).value_or(-1);

            const auto finish = [call, request]() {
                // Results of tools are filtered, description written by user is filtered too
                call->finishWithText(PrivacyFilter::instance()->filter(reportToMarkdown(request->data)));
            };
            const auto readJournal = [manager, call, request, finish]() {
                const QString executable = QFileInfo(request->data.crash.value("executable"_L1).toString()).fileName();
                if (executable.isEmpty()) {
                    finish();
                    return;
                }
                QMap<QString, QJsonValue> arguments{{u"executable"_s, executable}, {u"limit"_s, journalEntries}};
                // "Mon 2026-10-26 10:15:54 CET (21h ago)"
                static const QRegularExpression dateExpression(u"\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}"_s);
                const QString date = dateExpression.match(request->data.crash.value("dateTime"_L1).toString()).captured();
                if (const QDateTime crashTime = QDateTime::fromString(date, u"yyyy-MM-dd HH:mm:ss"_s); crashTime.isValid()) {
                    const QString format = u"yyyy-MM-dd HH:mm:ss"_s;
                    arguments.insert(u"since"_s, crashTime.addSecs(-journalMinutesBeforeCrash * 60).toString(format));
                    arguments.insert(u"until"_s, crashTime.addSecs(1).toString(format));
                }
                ToolHelpers::callTool(manager, u"read_journal"_s, arguments, call, [request, finish](const QJsonObject &result) {
                    request->data.journal = structuredContent(result);
                    finish();
                });
            };
            const auto readSystem = [manager, call, request, readJournal]() {
                ToolHelpers::callTool(manager, u"get_system_info"_s, {}, call, [request, readJournal](const QJsonObject &result) {
                    request->data.system = structuredContent(result);
                    readJournal();
                });
            };
            if (request->pid <= 0) {
                readSystem();
                return;
            }
            ToolHelpers::callTool(manager, u"get_crash_backtrace"_s, {{u"pid"_s, request->pid}}, call, [request, readSystem](const QJsonObject &result) {
                request->data.crash = structuredContent(result);
                readSystem();
            });
        });
}
