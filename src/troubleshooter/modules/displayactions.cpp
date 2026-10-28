/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "displayactions.h"
#include "backends/displaybackend.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QRegularExpression>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerTool>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr double minimumScale = 0.5;
constexpr double maximumScale = 3.0;
// Refresh rates are rounded in list_outputs (59.94)
constexpr double refreshRateTolerance = 1.0;

// Last outputs read: used to check arguments and to write summaries with modes
using OutputsCache = std::shared_ptr<QList<DisplayOutput>>;

// Same value when it's read again: compositor stores scale with its own precision
double roundedScale(double scale)
{
    return qRound(scale * 100) / 100.0;
}

const DisplayOutput *findOutput(const QList<DisplayOutput> &outputs, const QString &name)
{
    for (const DisplayOutput &output : outputs) {
        if (output.name == name) {
            return &output;
        }
    }
    return nullptr;
}

const DisplayMode *findOutputMode(const DisplayOutput &output, const QString &id)
{
    for (const DisplayMode &mode : output.modes) {
        if (mode.id == id) {
            return &mode;
        }
    }
    return nullptr;
}

QString sizeText(QSize size)
{
    return u"%1x%2"_s.arg(size.width()).arg(size.height());
}

// Size in desktop coordinates (positions are in this unit)
QSize logicalSize(const DisplayOutput &output)
{
    const DisplayMode *mode = findOutputMode(output, output.currentModeId);
    if (!mode || output.scale <= 0) {
        return {};
    }
    QSize size = (QSizeF(mode->size) / output.scale).toSize();
    if (output.rotation == "left"_L1 || output.rotation == "right"_L1) {
        size.transpose();
    }
    return size;
}

QJsonObject outputState(const DisplayOutput &output)
{
    if (!output.enabled) {
        return QJsonObject{{u"output"_s, output.name}, {u"enabled"_s, false}};
    }
    return QJsonObject{
        {u"output"_s, output.name},
        {u"enabled"_s, true},
        {u"mode"_s, output.currentModeId},
        {u"scale"_s, roundedScale(output.scale)},
        {u"x"_s, output.position.x()},
        {u"y"_s, output.position.y()},
    };
}

// State of a disabled output when it's enabled: native mode, at the right of other outputs
QJsonObject enabledState(const DisplayOutput &output, const QList<DisplayOutput> &outputs)
{
    QString modeId = output.preferredModeId;
    if (!findOutputMode(output, modeId)) {
        modeId = findOutputMode(output, output.currentModeId) || output.modes.isEmpty() ? output.currentModeId : output.modes.constFirst().id;
    }
    int x = 0;
    for (const DisplayOutput &other : outputs) {
        if (other.connected && other.enabled && other.name != output.name) {
            x = std::max(x, other.position.x() + logicalSize(other).width());
        }
    }
    return QJsonObject{
        {u"output"_s, output.name},
        {u"enabled"_s, true},
        {u"mode"_s, modeId},
        {u"scale"_s, output.scale > 0 ? roundedScale(output.scale) : 1.0},
        {u"x"_s, x},
        {u"y"_s, 0},
    };
}

bool hasArgument(const QJsonObject &arguments, QLatin1StringView name)
{
    const QJsonValue value = arguments.value(name);
    return !value.isUndefined() && !value.isNull();
}

QStringList availableSizes(const DisplayOutput &output)
{
    QStringList sizes;
    for (const DisplayMode &mode : output.modes) {
        if (const QString size = sizeText(mode.size); !sizes.contains(size)) {
            sizes.append(size);
        }
    }
    return sizes;
}

// Mode of \a size with refresh rate \a refreshRate (highest refresh rate when it's not given)
QString modeIdentifier(const DisplayOutput &output, QSize size, std::optional<double> refreshRate, QString *error)
{
    const DisplayMode *result = nullptr;
    QStringList refreshRates;
    for (const DisplayMode &mode : output.modes) {
        if (mode.size != size) {
            continue;
        }
        refreshRates.append(QString::number(qRound(mode.refreshRate * 100) / 100.0));
        if (!result) {
            result = &mode;
        } else if (refreshRate) {
            if (std::abs(mode.refreshRate - *refreshRate) < std::abs(result->refreshRate - *refreshRate)) {
                result = &mode;
            }
        } else if (mode.refreshRate > result->refreshRate) {
            result = &mode;
        }
    }
    if (!result) {
        *error = u"Output %1 has no resolution %2 (available: %3)"_s.arg(output.name, sizeText(size), availableSizes(output).join(u", "_s));
        return {};
    }
    if (refreshRate && std::abs(result->refreshRate - *refreshRate) > refreshRateTolerance) {
        *error = u"Output %1 has no refresh rate %2 Hz with resolution %3 (available: %4)"_s.arg(output.name,
                                                                                                 QString::number(*refreshRate),
                                                                                                 sizeText(size),
                                                                                                 refreshRates.join(u", "_s));
        return {};
    }
    return result->id;
}

QJsonObject outputTargetState(const QList<DisplayOutput> &outputs, const QJsonObject &arguments, const QJsonObject &current, QString *error)
{
    const DisplayOutput *output = findOutput(outputs, current.value("output"_L1).toString());
    if (!output) {
        *error = u"Unknown output: %1"_s.arg(current.value("output"_L1).toString());
        return {};
    }
    std::optional<bool> enabled;
    if (hasArgument(arguments, "enabled"_L1)) {
        enabled = ToolHelpers::booleanArgument(arguments.value("enabled"_L1));
        if (!enabled) {
            *error = u"enabled must be true or false"_s;
            return {};
        }
    }
    const bool changesSettings = std::ranges::any_of(std::initializer_list<QLatin1StringView>{"resolution"_L1, "refresh_rate"_L1, "scale"_L1, "x"_L1, "y"_L1},
                                                     [&arguments](QLatin1StringView name) {
                                                         return hasArgument(arguments, name);
                                                     });
    if (!enabled && !changesSettings) {
        *error = u"Give at least one of enabled, resolution, refresh_rate, scale, x or y"_s;
        return {};
    }
    if (enabled == false) {
        const bool otherEnabled = std::ranges::any_of(outputs, [output](const DisplayOutput &other) {
            return other.connected && other.enabled && other.name != output->name;
        });
        if (!otherEnabled) {
            *error = u"Refused: %1 is the only enabled output, nothing would be shown"_s.arg(output->name);
            return {};
        }
        return QJsonObject{{u"output"_s, output->name}, {u"enabled"_s, false}};
    }

    QJsonObject target = current.value("enabled"_L1).toBool() ? current : enabledState(*output, outputs);
    if (hasArgument(arguments, "resolution"_L1) || hasArgument(arguments, "refresh_rate"_L1)) {
        const DisplayMode *mode = findOutputMode(*output, target.value("mode"_L1).toString());
        QSize size = mode ? mode->size : QSize();
        if (hasArgument(arguments, "resolution"_L1)) {
            static const QRegularExpression resolutionExpression(u"^\\s*(\\d+)\\s*[x×]\\s*(\\d+)\\s*$"_s);
            const QRegularExpressionMatch match = resolutionExpression.match(arguments.value("resolution"_L1).toString());
            if (!match.hasMatch()) {
                *error = u"resolution must be like 1920x1080"_s;
                return {};
            }
            size = QSize(match.captured(1).toInt(), match.captured(2).toInt());
        }
        std::optional<double> refreshRate;
        if (hasArgument(arguments, "refresh_rate"_L1)) {
            refreshRate = ToolHelpers::numberArgument(arguments.value("refresh_rate"_L1));
            if (!refreshRate || *refreshRate <= 0) {
                *error = u"refresh_rate must be a number of Hz, e.g. 60"_s;
                return {};
            }
        }
        const QString modeId = modeIdentifier(*output, size, refreshRate, error);
        if (!error->isEmpty()) {
            return {};
        }
        target.insert(u"mode"_s, modeId);
    }
    if (hasArgument(arguments, "scale"_L1)) {
        const std::optional<double> scale = ToolHelpers::numberArgument(arguments.value("scale"_L1));
        if (!scale || *scale < minimumScale || *scale > maximumScale) {
            *error = u"scale must be between %1 and %2, e.g. 1.25 for 125%"_s.arg(minimumScale).arg(maximumScale);
            return {};
        }
        // Multiples of 5% like in System Settings
        target.insert(u"scale"_s, qRound(*scale * 20) * 5 / 100.0);
    }
    for (const QLatin1StringView coordinate : {"x"_L1, "y"_L1}) {
        if (hasArgument(arguments, coordinate)) {
            const std::optional<qint64> value = ToolHelpers::integerArgument(arguments.value(coordinate));
            if (!value) {
                *error = u"%1 must be an integer"_s.arg(coordinate);
                return {};
            }
            target.insert(coordinate, static_cast<int>(*value));
        }
    }
    return target;
}

QString stateDescription(const QList<DisplayOutput> &outputs, const QJsonObject &state)
{
    if (!state.value("enabled"_L1).toBool()) {
        return i18nc("@info display state", "disabled");
    }
    QString mode = state.value("mode"_L1).toString();
    if (const DisplayOutput *output = findOutput(outputs, state.value("output"_L1).toString()); output) {
        if (const DisplayMode *displayMode = findOutputMode(*output, mode); displayMode) {
            mode = u"%1@%2Hz"_s.arg(sizeText(displayMode->size)).arg(qRound(displayMode->refreshRate));
        }
    }
    return i18nc("@info display state, %1 mode, %2 scale percent, %3 and %4 position",
                 "%1, scale %2%, position %3,%4",
                 mode,
                 qRound(state.value("scale"_L1).toDouble() * 100),
                 // Coordinates without thousands separator
                 QString::number(state.value("x"_L1).toInt()),
                 QString::number(state.value("y"_L1).toInt()));
}

Action outputConfigAction(DisplayBackend *backend)
{
    auto cache = std::make_shared<QList<DisplayOutput>>();
    return Action{
        .readState =
            [backend, cache](const QJsonObject &arguments, QObject *context, const Action::StateCallback &callback) {
                backend->fetchOutputs(context, [cache, arguments, callback](const QList<DisplayOutput> &outputs, const QString &error) {
                    if (!error.isEmpty()) {
                        callback({}, error);
                        return;
                    }
                    *cache = outputs;
                    const QString name = arguments.value("output"_L1).toString();
                    const DisplayOutput *output = findOutput(outputs, name);
                    if (!output || !output->connected) {
                        callback({}, u"Unknown or disconnected output: %1 (use names of list_outputs)"_s.arg(name));
                        return;
                    }
                    callback(outputState(*output), {});
                });
            },
        .targetState =
            [cache](const QJsonObject &arguments, const QJsonObject &current, QString *error) {
                return outputTargetState(*cache, arguments, current, error);
            },
        .applyState =
            [backend](const QJsonObject &, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                const DisplayOutputSettings settings{
                    .enabled = state.value("enabled"_L1).toBool(),
                    .modeId = state.value("mode"_L1).toString(),
                    .scale = state.value("scale"_L1).toDouble(1.0),
                    .position = QPoint(state.value("x"_L1).toInt(), state.value("y"_L1).toInt()),
                };
                backend->applyOutputSettings(state.value("output"_L1).toString(), settings, context, callback);
            },
        .summary =
            [cache](const QJsonObject &before, const QJsonObject &after) {
                return i18nc("@info change, %1 output, %2 and %3 states",
                             "Display %1: %2 → %3",
                             after.value("output"_L1).toString(),
                             stateDescription(*cache, before),
                             stateDescription(*cache, after));
            },
    };
}
}

QList<std::pair<McpProtocolTool, Action>> DisplayActions::actions(DisplayBackend *backend)
{
    const ToolHelpers::ToolGroup group{u"display"_s, i18nc("@title tools group", "Displays")};
    const auto property = [](const QString &type, const QString &description) {
        return QJsonObject{{u"type"_s, type}, {u"description"_s, description}};
    };
    return {
        {ToolHelpers::createTool(u"set_output_config"_s,
                                 i18nc("@title tool", "Change Display Settings"),
                                 u"Enable or disable a display, or change its resolution, refresh rate, scale or position. Use names, resolutions and refresh "
                                 "rates given by list_outputs; only given settings are changed (an enabled display uses its native mode at the right of "
                                 "the others). Change is reverted after 15 seconds unless user keeps it: check \"kept\" or \"reverted\" in result. "
                                 "Disabling the only enabled display is refused."_s,
                                 ToolHelpers::ToolKind::ReversibleAction,
                                 group,
                                 {{u"output"_s, property(u"string"_s, u"Name of output given by list_outputs, e.g. HDMI-A-1"_s)},
                                  {u"enabled"_s, property(u"boolean"_s, u"true to enable output, false to disable it"_s)},
                                  {u"resolution"_s, property(u"string"_s, u"Resolution like 1920x1080"_s)},
                                  {u"refresh_rate"_s, property(u"number"_s, u"Refresh rate in Hz (default: highest for resolution)"_s)},
                                  {u"scale"_s, property(u"number"_s, u"Scale between %1 and %2, e.g. 1.25 for 125%"_s.arg(minimumScale).arg(maximumScale))},
                                  {u"x"_s, property(u"integer"_s, u"Horizontal position in desktop coordinates, like position of list_outputs"_s)},
                                  {u"y"_s, property(u"integer"_s, u"Vertical position in desktop coordinates"_s)}},
                                 {u"output"_s}),
         outputConfigAction(backend)},
    };
}

void DisplayActions::registerTools(McpProtocolServerProtocolManager *manager, DisplayBackend *backend, ActionRunner *runner)
{
    const QList<std::pair<McpProtocolTool, Action>> list = actions(backend);
    for (const auto &[definition, action] : list) {
        runner->addRevertibleActionTool(manager, definition, action);
    }
}
