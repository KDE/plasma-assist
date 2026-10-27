/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systemactions.h"
#include "backends/commandrunner.h"
#include "backends/networkbackend.h"
#include "backends/powerbackend.h"
#include "core/privacyfilter.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerTool>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

Action SystemActions::wifiAction(NetworkBackend *backend)
{
    return Action{
        .readState =
            [backend](const QJsonObject &, QObject *, const Action::StateCallback &callback) {
                if (!backend->isAvailable()) {
                    callback({}, u"NetworkManager is not running"_s);
                    return;
                }
                callback(QJsonObject{{u"wirelessEnabled"_s, backend->status().wirelessEnabled}}, {});
            },
        .targetState =
            [](const QJsonObject &arguments, const QJsonObject &, QString *error) {
                const std::optional<bool> enabled = ToolHelpers::booleanArgument(arguments.value("enabled"_L1));
                if (!enabled) {
                    *error = u"enabled must be true or false"_s;
                    return QJsonObject();
                }
                return QJsonObject{{u"wirelessEnabled"_s, *enabled}};
            },
        .applyState =
            [backend](const QJsonObject &, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                backend->setWirelessEnabled(state.value("wirelessEnabled"_L1).toBool(), context, callback);
            },
        .summary =
            [](const QJsonObject &, const QJsonObject &after) {
                return after.value("wirelessEnabled"_L1).toBool() ? i18nc("@info change", "Wi-Fi enabled") : i18nc("@info change", "Wi-Fi disabled");
            },
    };
}

Action SystemActions::powerProfileAction(PowerBackend *backend)
{
    auto choices = std::make_shared<QStringList>();
    return Action{
        .readState =
            [backend, choices](const QJsonObject &, QObject *context, const Action::StateCallback &callback) {
                backend->fetchStatus(context, [choices, callback](const PowerStatus &status, const QString &error) {
                    if (error.isEmpty() && status.powerProfileChoices.isEmpty()) {
                        callback({}, u"Power profiles are not supported on this computer"_s);
                        return;
                    }
                    *choices = status.powerProfileChoices;
                    callback(QJsonObject{{u"profile"_s, status.powerProfile}}, error);
                });
            },
        .targetState =
            [choices](const QJsonObject &arguments, const QJsonObject &, QString *error) {
                const QString profile = arguments.value("profile"_L1).toString();
                if (!choices->contains(profile)) {
                    *error = u"Unknown power profile %1, available profiles: %2"_s.arg(profile, choices->join(u", "_s));
                    return QJsonObject();
                }
                return QJsonObject{{u"profile"_s, profile}};
            },
        .applyState =
            [backend](const QJsonObject &, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                backend->setPowerProfile(state.value("profile"_L1).toString(), context, callback);
            },
        .summary =
            [](const QJsonObject &before, const QJsonObject &after) {
                return i18nc("@info change", "Power profile: %1 → %2", before.value("profile"_L1).toString(), after.value("profile"_L1).toString());
            },
    };
}

void SystemActions::registerTools(McpProtocolServerProtocolManager *manager,
                                  NetworkBackend *networkBackend,
                                  PowerBackend *powerBackend,
                                  CommandRunner *runner,
                                  ActionRunner *actionRunner)
{
    const ToolHelpers::ToolGroup networkGroup{u"network"_s, i18nc("@title tools group", "Network")};
    const ToolHelpers::ToolGroup powerGroup{u"power"_s, i18nc("@title tools group", "Power")};
    const ToolHelpers::ToolGroup desktopGroup{u"desktop"_s, i18nc("@title tools group", "Desktop")};

    actionRunner->addActionTool(manager,
                                ToolHelpers::createTool(u"set_wifi_enabled"_s,
                                                        i18nc("@title tool", "Enable or Disable Wi-Fi"),
                                                        u"Enable or disable Wi-Fi (it can't enable Wi-Fi disabled by a hardware switch)."_s,
                                                        ToolHelpers::ToolKind::ReversibleAction,
                                                        networkGroup,
                                                        {{u"enabled"_s, QJsonObject{{u"type"_s, u"boolean"_s}, {u"description"_s, u"true to enable Wi-Fi"_s}}}},
                                                        {u"enabled"_s}),
                                wifiAction(networkBackend));

    actionRunner->addActionTool(manager,
                                ToolHelpers::createTool(u"set_power_profile"_s,
                                                        i18nc("@title tool", "Change Power Profile"),
                                                        u"Change power profile: power-saver, balanced or performance (choices of get_power_status)."_s,
                                                        ToolHelpers::ToolKind::ReversibleAction,
                                                        powerGroup,
                                                        {{u"profile"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Power profile"_s}}}},
                                                        {u"profile"_s}),
                                powerProfileAction(powerBackend));

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(u"reconnect_network"_s,
                                i18nc("@title tool", "Reconnect Network"),
                                u"Disconnect then connect again a saved network connection (name given by list_connections). Network is unavailable "
                                "for a few seconds."_s,
                                ToolHelpers::ToolKind::ReversibleAction,
                                networkGroup,
                                {{u"connection"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Name of connection"_s}}}},
                                {u"connection"_s}),
        [networkBackend, actionRunner](McpProtocolServerToolCall *call) {
            const QJsonValue argument = PrivacyFilter::instance()->restore(call->arguments().value(u"connection"_s));
            const QString connection = argument.toString();
            if (!networkBackend->isAvailable()) {
                ToolHelpers::finishWithError(call, u"NetworkManager is not running"_s);
                return;
            }
            networkBackend->reconnect(connection, call, [call, connection, actionRunner](const QString &error) {
                if (!error.isEmpty()) {
                    ToolHelpers::finishWithError(call, error);
                    return;
                }
                const QString summary = i18nc("@info change", "Connection %1 reconnected", connection);
                actionRunner->record(u"reconnect_network"_s, summary, QJsonObject{{u"connection"_s, connection}});
                ToolHelpers::finishWithJson(call, QJsonObject{{u"changed"_s, true}, {u"summary"_s, summary}}, summary);
            });
        });

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"restart_plasmashell"_s,
                                                         i18nc("@title tool", "Restart Plasma Shell"),
                                                         u"Restart Plasma shell (panels, desktop, applets): use it only when panels or desktop are frozen "
                                                         "or broken. Applets are closed, including this assistant."_s,
                                                         ToolHelpers::ToolKind::DestructiveAction,
                                                         desktopGroup),
                                 [runner, actionRunner](McpProtocolServerToolCall *call) {
                                     runner->run(CommandRunner::Command::Systemctl,
                                                 {u"--user"_s, u"restart"_s, u"--no-block"_s, u"plasma-plasmashell.service"_s},
                                                 call,
                                                 [call, actionRunner](const QByteArray &, const QString &error) {
                                                     if (!error.isEmpty()) {
                                                         ToolHelpers::finishWithError(call, error);
                                                         return;
                                                     }
                                                     const QString summary = i18nc("@info change", "Plasma shell restarted");
                                                     actionRunner->record(u"restart_plasmashell"_s, summary, {});
                                                     ToolHelpers::finishWithJson(call, QJsonObject{{u"changed"_s, true}, {u"summary"_s, summary}}, summary);
                                                 });
                                 });
}
