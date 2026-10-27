/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "powermodule.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QJsonArray>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr int lowBatteryPercent = 15;
constexpr int wornBatteryPercent = 60;
}

QJsonObject PowerModule::statusToJson(const PowerStatus &status)
{
    QJsonArray batteries;
    for (const BatteryInfo &battery : status.batteries) {
        batteries.append(QJsonObject{
            {u"name"_s, battery.name},
            {u"type"_s, battery.type},
            {u"chargeState"_s, battery.chargeState},
            {u"chargePercent"_s, battery.chargePercent},
            {u"capacityPercent"_s, battery.capacityPercent},
            {u"timeToEmptyMinutes"_s, battery.timeToEmptySec / 60},
            {u"present"_s, battery.present},
            {u"powerSupply"_s, battery.powerSupply},
        });
    }
    QJsonObject settings;
    for (auto it = status.settings.cbegin(); it != status.settings.cend(); ++it) {
        QJsonObject entries;
        for (auto entry = it.value().cbegin(); entry != it.value().cend(); ++entry) {
            entries.insert(entry.key(), entry.value());
        }
        settings.insert(it.key(), entries);
    }
    return QJsonObject{
        {u"onBattery"_s, status.onBattery},
        {u"batteries"_s, batteries},
        {u"powerProfile"_s, status.powerProfile},
        {u"powerProfileChoices"_s, QJsonArray::fromStringList(status.powerProfileChoices)},
        // Settings which are not listed have their default value
        {u"changedSettings"_s, settings},
        {u"hints"_s, QJsonArray::fromStringList(hints(status))},
    };
}

QList<SleepInhibitor> PowerModule::blockingInhibitors(const QList<SleepInhibitor> &inhibitors)
{
    QList<SleepInhibitor> result;
    for (const SleepInhibitor &inhibitor : inhibitors) {
        // logind "delay" inhibitors only delay sleep a few seconds (cleanup before suspend)
        if (inhibitor.mode != "block"_L1) {
            continue;
        }
        const QStringList what = inhibitor.what.split(u':');
        if (what.contains("sleep"_L1) || what.contains("idle"_L1)) {
            result.append(inhibitor);
        }
    }
    return result;
}

QJsonObject PowerModule::inhibitorsToJson(const QList<SleepInhibitor> &inhibitors)
{
    QJsonArray array;
    for (const SleepInhibitor &inhibitor : inhibitors) {
        QJsonObject object{
            {u"application"_s, inhibitor.who},
            {u"reason"_s, inhibitor.why},
            {u"what"_s, inhibitor.what},
            {u"source"_s, inhibitor.source},
        };
        if (inhibitor.pid > 0) {
            object.insert(u"pid"_s, static_cast<qint64>(inhibitor.pid));
        }
        array.append(object);
    }
    return QJsonObject{{u"inhibitors"_s, array}};
}

QStringList PowerModule::hints(const PowerStatus &status)
{
    QStringList result;
    for (const BatteryInfo &battery : status.batteries) {
        if (!battery.powerSupply || !battery.present) {
            continue;
        }
        if (battery.chargeState == "Discharging"_L1 && battery.chargePercent < lowBatteryPercent) {
            result.append(u"Battery %1 is low (%2%)"_s.arg(battery.name).arg(battery.chargePercent));
        }
        if (battery.capacityPercent > 0 && battery.capacityPercent < wornBatteryPercent) {
            result.append(u"Battery %1 is worn: %2% of its design capacity"_s.arg(battery.name).arg(battery.capacityPercent));
        }
    }
    // AutoSuspendAction 0: do nothing when idle
    for (const QString &profile : {u"AC"_s, u"Battery"_s}) {
        if (status.settings.value(profile + u"/SuspendAndShutdown"_s).value(u"AutoSuspendAction"_s) == "0"_L1) {
            result.append(u"Automatic sleep is disabled in profile %1"_s.arg(profile));
        }
    }
    if (status.onBattery && status.powerProfile == "performance"_L1) {
        result.append(u"Performance power profile is used on battery: battery drains faster"_s);
    }
    return result;
}

void PowerModule::registerTools(McpProtocolServerProtocolManager *manager, PowerBackend *backend)
{
    const ToolHelpers::ToolGroup group{u"power"_s, i18nc("@title tools group", "Power")};

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"get_power_status"_s,
                                                         i18nc("@title tool", "Power Status"),
                                                         u"Get power state: on battery or AC, batteries (charge, health), power profile, and sleep "
                                                         "and screen settings changed by the user. Result contains hints about obvious problems. Use it "
                                                         "when computer doesn't sleep, sleeps too early or battery drains fast."_s,
                                                         ToolHelpers::ToolKind::Diagnostic,
                                                         group),
                                 [backend](McpProtocolServerToolCall *call) {
                                     backend->fetchStatus(call, [call](const PowerStatus &status, const QString &error) {
                                         if (!error.isEmpty()) {
                                             ToolHelpers::finishWithError(call, error);
                                             return;
                                         }
                                         QString summary = status.onBattery ? u"On battery"_s : u"On AC power"_s;
                                         if (!status.powerProfile.isEmpty()) {
                                             summary += u", power profile %1"_s.arg(status.powerProfile);
                                         }
                                         if (const QStringList problems = hints(status); !problems.isEmpty()) {
                                             summary += u". "_s + problems.join(u". "_s);
                                         }
                                         ToolHelpers::finishWithJson(call, statusToJson(status), summary);
                                     });
                                 });

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"list_sleep_inhibitors"_s,
                                                         i18nc("@title tool", "Sleep Inhibitors"),
                                                         u"List applications which currently prevent the computer from sleeping or the screen from "
                                                         "turning off (video player, presentation, activity settings…), with their reason. Use it when "
                                                         "computer doesn't sleep."_s,
                                                         ToolHelpers::ToolKind::Diagnostic,
                                                         group),
                                 [backend](McpProtocolServerToolCall *call) {
                                     backend->fetchInhibitors(call, [call](const QList<SleepInhibitor> &inhibitors, const QString &error) {
                                         if (!error.isEmpty()) {
                                             ToolHelpers::finishWithError(call, error);
                                             return;
                                         }
                                         const QList<SleepInhibitor> blocking = blockingInhibitors(inhibitors);
                                         QStringList applications;
                                         for (const SleepInhibitor &inhibitor : blocking) {
                                             applications.append(inhibitor.who);
                                         }
                                         applications.removeDuplicates();
                                         ToolHelpers::finishWithJson(call,
                                                                     inhibitorsToJson(blocking),
                                                                     blocking.isEmpty() ? u"No application prevents sleep"_s
                                                                                        : u"Sleep is prevented by: %1"_s.arg(applications.join(u", "_s)));
                                     });
                                 });
}
