/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "displaymodule.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QJsonArray>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
// Keeps result small: an output can have dozens of modes
constexpr qsizetype maximumModeSizes = 10;

const DisplayMode *findMode(const DisplayOutput &output, const QString &id)
{
    for (const DisplayMode &mode : output.modes) {
        if (mode.id == id) {
            return &mode;
        }
    }
    return nullptr;
}

QString sizeToString(QSize size)
{
    return u"%1x%2"_s.arg(size.width()).arg(size.height());
}

QJsonValue modeToJson(const DisplayMode *mode)
{
    if (!mode) {
        return QJsonValue::Null;
    }
    return QJsonObject{
        {u"size"_s, sizeToString(mode->size)},
        {u"refreshRate"_s, qRound(mode->refreshRate * 100) / 100.0},
    };
}

QString modeToString(const DisplayMode *mode)
{
    return u"%1@%2Hz"_s.arg(sizeToString(mode->size)).arg(qRound(mode->refreshRate));
}

// Distinct sizes (largest first) with their refresh rates
QJsonArray availableModes(const DisplayOutput &output)
{
    QList<DisplayMode> modes = output.modes;
    std::sort(modes.begin(), modes.end(), [](const DisplayMode &left, const DisplayMode &right) {
        const qint64 leftArea = qint64(left.size.width()) * left.size.height();
        const qint64 rightArea = qint64(right.size.width()) * right.size.height();
        return leftArea != rightArea ? leftArea > rightArea : left.refreshRate > right.refreshRate;
    });
    QJsonArray result;
    QSize previousSize;
    QJsonArray refreshRates;
    for (const DisplayMode &mode : std::as_const(modes)) {
        if (mode.size != previousSize) {
            if (previousSize.isValid()) {
                result.append(QJsonObject{{u"size"_s, sizeToString(previousSize)}, {u"refreshRates"_s, refreshRates}});
                if (result.count() == maximumModeSizes) {
                    return result;
                }
            }
            previousSize = mode.size;
            refreshRates = {};
        }
        const double refreshRate = qRound(mode.refreshRate * 100) / 100.0;
        if (!refreshRates.contains(refreshRate)) {
            refreshRates.append(refreshRate);
        }
    }
    if (previousSize.isValid()) {
        result.append(QJsonObject{{u"size"_s, sizeToString(previousSize)}, {u"refreshRates"_s, refreshRates}});
    }
    return result;
}
}

QJsonObject DisplayModule::outputsToJson(const QList<DisplayOutput> &outputs)
{
    QJsonArray connected;
    QJsonArray disconnected;
    for (const DisplayOutput &output : outputs) {
        if (!output.connected) {
            disconnected.append(output.name);
            continue;
        }
        connected.append(QJsonObject{
            {u"id"_s, output.id},
            {u"name"_s, output.name},
            {u"type"_s, output.type},
            {u"vendor"_s, output.vendor},
            {u"model"_s, output.model},
            {u"enabled"_s, output.enabled},
            {u"primary"_s, output.priority == 1},
            {u"priority"_s, static_cast<qint64>(output.priority)},
            {u"currentMode"_s, modeToJson(findMode(output, output.currentModeId))},
            {u"preferredMode"_s, modeToJson(findMode(output, output.preferredModeId))},
            {u"availableModes"_s, availableModes(output)},
            {u"scale"_s, output.scale},
            {u"rotation"_s, output.rotation},
            {u"position"_s, QJsonObject{{u"x"_s, output.position.x()}, {u"y"_s, output.position.y()}}},
        });
    }
    return QJsonObject{
        {u"outputs"_s, connected},
        {u"disconnectedOutputs"_s, disconnected},
        {u"hints"_s, QJsonArray::fromStringList(hints(outputs))},
    };
}

QStringList DisplayModule::hints(const QList<DisplayOutput> &outputs)
{
    QStringList result;
    bool hasEnabledOutput = false;
    for (const DisplayOutput &output : outputs) {
        if (!output.connected) {
            continue;
        }
        if (!output.enabled) {
            result.append(u"Output %1 is connected but disabled"_s.arg(output.name));
            continue;
        }
        hasEnabledOutput = true;
        const DisplayMode *current = findMode(output, output.currentModeId);
        const DisplayMode *preferred = findMode(output, output.preferredModeId);
        if (current && preferred && current->size != preferred->size) {
            result.append(u"Output %1 uses mode %2 instead of its native mode %3"_s.arg(output.name, modeToString(current), modeToString(preferred)));
        }
        if (output.rotation != "none"_L1) {
            result.append(u"Output %1 is rotated (%2)"_s.arg(output.name, output.rotation));
        }
    }
    if (!hasEnabledOutput) {
        result.append(u"No connected output is enabled"_s);
    }
    return result;
}

void DisplayModule::registerTools(McpProtocolServerProtocolManager *manager, DisplayBackend *backend)
{
    const ToolHelpers::ToolGroup group{u"display"_s, i18nc("@title tools group", "Displays")};

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(u"list_outputs"_s,
                                i18nc("@title tool", "Displays"),
                                u"List displays: connected and enabled state, current and native (preferred) mode, available modes, scale, rotation, "
                                "position and primary display. Result contains hints about obvious problems. "
                                "Use it first when an external screen shows nothing, is blurry or badly placed."_s,
                                ToolHelpers::ToolKind::Diagnostic,
                                group),
        [backend](McpProtocolServerToolCall *call) {
            backend->fetchOutputs(call, [call](const QList<DisplayOutput> &outputs, const QString &error) {
                if (!error.isEmpty()) {
                    call->finishWithText(error, true);
                    return;
                }
                const qsizetype connectedCount = std::count_if(outputs.cbegin(), outputs.cend(), [](const DisplayOutput &output) {
                    return output.connected;
                });
                const QStringList problems = hints(outputs);
                QString summary = u"%1 connected outputs"_s.arg(connectedCount);
                if (!problems.isEmpty()) {
                    summary += u". "_s + problems.join(u". "_s);
                }
                ToolHelpers::finishWithJson(call, outputsToJson(outputs), summary);
            });
        });
}
