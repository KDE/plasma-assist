/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "crashmodule.h"
#include "backends/commandrunner.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr int defaultDays = 7;
constexpr int maximumDays = 90;
constexpr int defaultLimit = 10;
constexpr int maximumLimit = 50;
constexpr int maximumFrames = 40;
// A crash loop can create thousands of crashes: only most recent ones are counted
constexpr int maximumCountedCrashes = 2000;

// coredumpctl exits with an error when no crash is found
bool isNoCrashError(const QString &error)
{
    return error.contains("No coredumps found"_L1, Qt::CaseInsensitive);
}
}

QString CrashModule::signalName(int signal)
{
    static const QMap<int, QString> names{
        {4, u"SIGILL"_s},
        {5, u"SIGTRAP"_s},
        {6, u"SIGABRT"_s},
        {7, u"SIGBUS"_s},
        {8, u"SIGFPE"_s},
        {9, u"SIGKILL"_s},
        {11, u"SIGSEGV"_s},
        {15, u"SIGTERM"_s},
    };
    return names.value(signal, QString::number(signal));
}

QJsonObject CrashModule::crashesToJson(const QByteArray &output, int limit)
{
    QJsonArray crashes;
    QMap<QString, int> counts;
    const QJsonArray entries = QJsonDocument::fromJson(output).array();
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        const QString executable = entry.value("exe"_L1).toString();
        const QString application = QFileInfo(executable).fileName();
        ++counts[application];
        if (crashes.count() >= limit) {
            continue;
        }
        crashes.append(QJsonObject{
            {u"application"_s, application},
            {u"executable"_s, executable},
            {u"pid"_s, entry.value("pid"_L1).toInteger()},
            {u"dateTime"_s, QDateTime::fromMSecsSinceEpoch(entry.value("time"_L1).toInteger() / 1000).toString(Qt::ISODate)},
            {u"signal"_s, signalName(entry.value("sig"_L1).toInt())},
            // present, missing (removed or too big)…
            {u"coreFile"_s, entry.value("corefile"_L1).toString()},
        });
    }
    // Applications which crash often first
    QList<std::pair<QString, int>> sortedCounts;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        sortedCounts.append({it.key(), it.value()});
    }
    std::ranges::stable_sort(sortedCounts, [](const auto &left, const auto &right) {
        return left.second > right.second;
    });
    QJsonArray countsArray;
    for (const auto &[application, count] : std::as_const(sortedCounts)) {
        countsArray.append(QJsonObject{{u"application"_s, application}, {u"count"_s, count}});
    }
    return QJsonObject{
        {u"total"_s, entries.count()},
        // Total is the maximum number of crashes read: there are more crashes
        {u"totalIsCapped"_s, entries.count() >= maximumCountedCrashes},
        {u"crashes"_s, crashes},
        {u"crashesByApplication"_s, countsArray},
    };
}

QJsonObject CrashModule::backtraceToJson(const QByteArray &output, int maximumFrames)
{
    // Fields useful to diagnose (machine id, boot id, control group… are not).
    // Command line is not kept: it can contain secrets (tokens in URLs…)
    static const QMap<QString, QString> fields{
        {u"PID"_s, u"pid"_s},
        {u"Signal"_s, u"signal"_s},
        {u"Timestamp"_s, u"dateTime"_s},
        {u"Executable"_s, u"executable"_s},
        {u"Storage"_s, u"storage"_s},
    };
    QJsonObject result;
    QJsonArray frames;
    QString thread;
    int otherThreads = 0;
    bool inStackTrace = false;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.startsWith("Stack trace of thread"_L1)) {
            // First thread is the crashed one
            if (thread.isEmpty()) {
                thread = line.section(u' ', 4).chopped(1);
                inStackTrace = true;
            } else {
                ++otherThreads;
                inStackTrace = false;
            }
            continue;
        }
        if (inStackTrace) {
            if (line.startsWith(u'#')) {
                if (frames.count() < maximumFrames) {
                    // "#0  0x00007f… function (library + 0x…)": address is useless for a reader
                    QString frame = line.section(u' ', 0, 0) + u' ' + line.section(u' ', 1, -1, QString::SectionSkipEmpty).section(u' ', 1);
                    frames.append(frame.simplified());
                }
                continue;
            }
            inStackTrace = false;
        }
        if (const qsizetype colon = line.indexOf(u':'); colon > 0) {
            if (const QString key = fields.value(line.left(colon)); !key.isEmpty() && !result.contains(key)) {
                result.insert(key, line.mid(colon + 1).trimmed());
            }
        }
    }
    if (result.isEmpty()) {
        return {};
    }
    result.insert(u"thread"_s, thread);
    result.insert(u"frames"_s, frames);
    result.insert(u"otherThreads"_s, otherThreads);
    result.insert(u"hasBacktrace"_s, !frames.isEmpty());
    return result;
}

void CrashModule::registerTools(McpProtocolServerProtocolManager *manager, CommandRunner *runner)
{
    const ToolHelpers::ToolGroup group{u"crashes"_s, i18nc("@title tools group", "Crashes")};

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(
            u"list_recent_crashes"_s,
            i18nc("@title tool", "Recent Crashes"),
            u"List applications which crashed recently (systemd-coredump): application, date, signal, and number of crashes by "
            "application. Use it when an application crashes or closes unexpectedly, then get_crash_backtrace with its pid."_s,
            ToolHelpers::ToolKind::Diagnostic,
            group,
            {
                {u"days"_s,
                 QJsonObject{{u"type"_s, u"integer"_s},
                             {u"description"_s, u"Crashes of the last days (default %1, maximum %2)"_s.arg(defaultDays).arg(maximumDays)}}},
                {u"limit"_s,
                 QJsonObject{{u"type"_s, u"integer"_s},
                             {u"description"_s, u"Maximum number of crashes (default %1, maximum %2)"_s.arg(defaultLimit).arg(maximumLimit)}}},
            }),
        [runner](McpProtocolServerToolCall *call) {
            const int days = std::clamp(call->arguments().value(u"days"_s).toInt(defaultDays), 1, maximumDays);
            const int limit = std::clamp(call->arguments().value(u"limit"_s).toInt(defaultLimit), 1, maximumLimit);
            runner->run(
                CommandRunner::Command::Coredumpctl,
                {u"--json=short"_s, u"--no-pager"_s, u"-q"_s, u"-r"_s, u"-n"_s, QString::number(maximumCountedCrashes), u"--since=-%1d"_s.arg(days), u"list"_s},
                call,
                [call, limit, days](const QByteArray &output, const QString &error) {
                    if (!error.isEmpty() && !isNoCrashError(error)) {
                        ToolHelpers::finishWithError(call, error);
                        return;
                    }
                    const QJsonObject result = crashesToJson(output, limit);
                    QString summary = u"%1%2 crashes in the last %3 days"_s.arg(result.value("totalIsCapped"_L1).toBool() ? u"More than "_s : QString())
                                          .arg(result.value("total"_L1).toInt())
                                          .arg(days);
                    if (const QJsonArray counts = result.value("crashesByApplication"_L1).toArray(); !counts.isEmpty()) {
                        const QJsonObject first = counts.first().toObject();
                        summary += u", most often %1 (%2 times)"_s.arg(first.value("application"_L1).toString()).arg(first.value("count"_L1).toInt());
                    }
                    ToolHelpers::finishWithJson(call, result, summary);
                });
        });

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(
            u"get_crash_backtrace"_s,
            i18nc("@title tool", "Crash Backtrace"),
            u"Get the stack trace of the thread which crashed, for a crash listed by list_recent_crashes. "
            "Use it to explain the crash or to prepare a bug report."_s,
            ToolHelpers::ToolKind::Diagnostic,
            group,
            {{u"pid"_s, QJsonObject{{u"type"_s, u"integer"_s}, {u"description"_s, u"Process id of the crash (pid of list_recent_crashes)"_s}}}},
            {u"pid"_s}),
        [runner](McpProtocolServerToolCall *call) {
            const qint64 pid = call->arguments().value(u"pid"_s).toInteger(-1);
            if (pid <= 0) {
                ToolHelpers::finishWithError(call, u"Invalid pid"_s);
                return;
            }
            runner->run(CommandRunner::Command::Coredumpctl,
                        {u"--no-pager"_s, u"-q"_s, u"info"_s, QString::number(pid)},
                        call,
                        [call, pid](const QByteArray &output, const QString &error) {
                            if (!error.isEmpty()) {
                                ToolHelpers::finishWithError(call, isNoCrashError(error) ? u"No crash found with pid %1"_s.arg(pid) : error);
                                return;
                            }
                            const QJsonObject result = backtraceToJson(output, maximumFrames);
                            if (result.isEmpty()) {
                                ToolHelpers::finishWithError(call, u"No crash found with pid %1"_s.arg(pid));
                                return;
                            }
                            ToolHelpers::finishWithJson(
                                call,
                                result,
                                result.value("hasBacktrace"_L1).toBool()
                                    ? u"Crash of %1: %2 frames"_s.arg(result.value("executable"_L1).toString()).arg(result.value("frames"_L1).toArray().count())
                                    : u"Crash of %1: no backtrace available"_s.arg(result.value("executable"_L1).toString()));
                        });
        });
}
