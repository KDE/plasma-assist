/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "logmodule.h"
#include "backends/commandrunner.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerTool>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr int defaultLimit = 50;
constexpr int maximumLimit = 200;
// Long messages (dumps…) are cut
constexpr qsizetype maximumMessageLength = 500;

const QStringList &priorities()
{
    static const QStringList list{u"emerg"_s, u"alert"_s, u"crit"_s, u"err"_s, u"warning"_s, u"notice"_s, u"info"_s, u"debug"_s};
    return list;
}

// MESSAGE is an array of bytes when it's not valid UTF-8
QString message(const QJsonValue &value)
{
    if (value.isArray()) {
        QByteArray bytes;
        const QJsonArray array = value.toArray();
        for (const QJsonValue &byte : array) {
            bytes.append(static_cast<char>(byte.toInt()));
        }
        return QString::fromUtf8(bytes);
    }
    return value.toString();
}
}

LogModule::JournalArguments LogModule::journalArguments(const QMap<QString, QJsonValue> &toolArguments)
{
    // Values are given as separate arguments (no shell), they are checked to not be read as options
    static const QRegularExpression unitExpression(u"^[A-Za-z0-9@._:\\\\-]+$"_s);
    static const QRegularExpression executableExpression(u"^[A-Za-z0-9._+\\-]+$"_s);
    static const QRegularExpression sinceExpression(u"^(-?\\d+(s|min|h|d|w)|today|yesterday|\\d{4}-\\d{2}-\\d{2}( \\d{2}:\\d{2}(:\\d{2})?)?)$"_s);

    JournalArguments result;
    result.limit = static_cast<int>(std::clamp<qint64>(ToolHelpers::integerArgument(toolArguments.value(u"limit"_s)).value_or(defaultLimit), 1, maximumLimit));
    const bool system = ToolHelpers::booleanArgument(toolArguments.value(u"system"_s)).value_or(false);
    result.arguments = {u"-o"_s, u"json"_s, u"--no-pager"_s, u"-q"_s, u"-n"_s, QString::number(result.limit)};
    if (!system) {
        result.arguments.append(u"--user"_s);
    }
    if (const QString unit = toolArguments.value(u"unit"_s).toString(); !unit.isEmpty()) {
        if (!unitExpression.match(unit).hasMatch() || unit.startsWith(u'-')) {
            result.error = u"Invalid unit: %1"_s.arg(unit);
            return result;
        }
        result.arguments += {u"-u"_s, unit};
    }
    if (const QString executable = toolArguments.value(u"executable"_s).toString(); !executable.isEmpty()) {
        if (!executableExpression.match(executable).hasMatch() || executable.startsWith(u'-')) {
            result.error = u"Invalid executable name: %1"_s.arg(executable);
            return result;
        }
        // Matches name of process (journalctl only knows full paths for _EXE)
        result.arguments.append(u"_COMM=%1"_s.arg(executable.left(15)));
    }
    if (const QString since = toolArguments.value(u"since"_s).toString(); !since.isEmpty()) {
        if (!sinceExpression.match(since).hasMatch()) {
            result.error = u"Invalid since value: %1 (examples: 1h, 30min, 2d, today, 2026-10-27 08:00)"_s.arg(since);
            return result;
        }
        // "1h" means one hour ago
        result.arguments.append(u"--since=%1"_s.arg(since.at(0).isDigit() && !since.contains(u'-') ? u'-' + since : since));
    }
    if (const QString until = toolArguments.value(u"until"_s).toString(); !until.isEmpty()) {
        if (!sinceExpression.match(until).hasMatch()) {
            result.error = u"Invalid until value: %1 (examples: 1h, today, 2026-10-27 08:00)"_s.arg(until);
            return result;
        }
        result.arguments.append(u"--until=%1"_s.arg(until.at(0).isDigit() && !until.contains(u'-') ? u'-' + until : until));
    }
    if (const QString priority = toolArguments.value(u"priority"_s).toString(); !priority.isEmpty()) {
        if (!priorities().contains(priority)) {
            result.error = u"Invalid priority: %1"_s.arg(priority);
            return result;
        }
        result.arguments += {u"-p"_s, priority};
    }
    return result;
}

QJsonObject LogModule::entriesToJson(const QByteArray &output)
{
    QJsonArray entries;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &line : lines) {
        const QJsonObject entry = QJsonDocument::fromJson(line).object();
        if (entry.isEmpty()) {
            continue;
        }
        QString text = message(entry.value("MESSAGE"_L1));
        if (text.size() > maximumMessageLength) {
            text = text.left(maximumMessageLength) + u'…';
        }
        QString identifier = entry.value("SYSLOG_IDENTIFIER"_L1).toString();
        if (identifier.isEmpty()) {
            identifier = entry.value("_COMM"_L1).toString();
        }
        entries.append(QJsonObject{
            {u"dateTime"_s, QDateTime::fromMSecsSinceEpoch(entry.value("__REALTIME_TIMESTAMP"_L1).toString().toLongLong() / 1000).toString(Qt::ISODateWithMs)},
            {u"priority"_s, priorities().value(entry.value("PRIORITY"_L1).toString().toInt(), u"info"_s)},
            {u"identifier"_s, identifier},
            {u"pid"_s, entry.value("_PID"_L1).toString()},
            {u"message"_s, text},
        });
    }
    return QJsonObject{{u"count"_s, entries.count()}, {u"entries"_s, entries}};
}

void LogModule::registerTools(McpProtocolServerProtocolManager *manager, CommandRunner *runner)
{
    const ToolHelpers::ToolGroup group{u"logs"_s, i18nc("@title tools group", "Logs")};

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(
            u"read_journal"_s,
            i18nc("@title tool", "System Logs"),
            u"Read last entries of the journal (logs), filtered by unit, executable, date and priority. By default it reads the journal of "
            "the user session; set system to true for system services (NetworkManager, bluetooth…), it may be empty when user can't read it. "
            "Use filters: results are limited to %1 entries."_s.arg(maximumLimit),
            ToolHelpers::ToolKind::Diagnostic,
            group,
            {
                {u"unit"_s,
                 QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"systemd unit, e.g. NetworkManager.service or plasma-plasmashell.service"_s}}},
                {u"executable"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Name of process, e.g. plasmashell or kwin_wayland"_s}}},
                {u"since"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Entries since: 30min, 1h, 2d, today, yesterday or a date"_s}}},
                {u"until"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Entries until: same format as since"_s}}},
                {u"priority"_s, McpProtocolServerTool::schemaEnumProperty(priorities(), u"Maximum priority: err shows emerg, alert, crit and err entries"_s)},
                {u"limit"_s,
                 QJsonObject{{u"type"_s, u"integer"_s},
                             {u"description"_s, u"Maximum number of entries (default %1, maximum %2)"_s.arg(defaultLimit).arg(maximumLimit)}}},
                {u"system"_s, QJsonObject{{u"type"_s, u"boolean"_s}, {u"description"_s, u"Read system journal instead of user journal"_s}}},
            }),
        [runner](McpProtocolServerToolCall *call) {
            const JournalArguments arguments = journalArguments(call->arguments());
            if (!arguments.error.isEmpty()) {
                ToolHelpers::finishWithError(call, arguments.error);
                return;
            }
            const bool system = ToolHelpers::booleanArgument(call->arguments().value(u"system"_s)).value_or(false);
            runner->run(CommandRunner::Command::Journalctl, arguments.arguments, call, [call, system](const QByteArray &output, const QString &error) {
                if (!error.isEmpty()) {
                    ToolHelpers::finishWithError(call, error);
                    return;
                }
                const QJsonObject result = entriesToJson(output);
                const int count = result.value("count"_L1).toInt();
                QString summary = u"%1 journal entries"_s.arg(count);
                if (count == 0 && system) {
                    summary += u" (system journal can only be read by members of groups systemd-journal, adm or wheel)"_s;
                }
                ToolHelpers::finishWithJson(call, result, summary);
            });
        });
}
