/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scenarioprompts.h"
#include <KLocalizedString>
#include <QJsonArray>
#include <QLocale>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolMeta>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerPrompt>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
class ScenarioPrompt : public McpProtocolServerPrompt
{
public:
    explicit ScenarioPrompt(const ScenarioPrompts::Scenario &scenario)
        : mScenario(scenario)
    {
    }

    [[nodiscard]] McpProtocolPrompt definition() const override
    {
        McpProtocolPrompt prompt =
            createPrompt(mScenario.name, mScenario.description, {createArgument(u"details"_s, u"Description of the problem given by user (optional)"_s)});
        prompt.setTitle(mScenario.title);
        McpProtocolMeta meta;
        meta.setMeta(QMap<QString, QJsonValue>{
            {u"org.kde/icon"_s, mScenario.iconName},
            {u"org.kde/diagnosticTools"_s, QJsonArray::fromStringList(mScenario.diagnosticTools)},
        });
        prompt.setMeta(meta);
        return prompt;
    }

    [[nodiscard]] McpProtocolGetPromptResult get(const QMap<QString, QString> &arguments) const override
    {
        // Language model answers in language of user
        const QLocale locale;
        const QString language = u"%1 (%2)"_s.arg(QLocale::languageToString(locale.language()), locale.nativeLanguageName());
        return createUserTextResult(ScenarioPrompts::promptText(mScenario, arguments.value(u"details"_s), language), mScenario.description);
    }

private:
    const ScenarioPrompts::Scenario mScenario;
};
}

QList<ScenarioPrompts::Scenario> ScenarioPrompts::scenarios()
{
    return {
        {
            .name = u"no_sound"_s,
            .title = i18nc("@title scenario", "No Sound"),
            .description = u"Diagnose and fix missing sound (speakers, headphones, HDMI, Bluetooth)"_s,
            .iconName = u"audio-volume-muted"_s,
            .request = u"I have no sound."_s,
            .steps = {u"list_audio_devices: default output, muted state, volume and plugged ports"_s,
                      u"list_audio_cards: profiles (HDMI, analog, Bluetooth A2DP or headset)"_s,
                      u"list_audio_streams: when only one application has no sound"_s},
            .fixes = {u"set_default_audio_device to use plugged headphones or HDMI"_s,
                      u"set_audio_mute or set_audio_volume"_s,
                      u"set_card_profile when an output is missing or a Bluetooth headset sounds bad"_s,
                      u"move_audio_stream when an application uses another output"_s},
            .diagnosticTools = {u"list_audio_devices"_s, u"list_audio_cards"_s, u"list_audio_streams"_s},
        },
        {
            .name = u"external_display"_s,
            .title = i18nc("@title scenario", "External Screen"),
            .description = u"Diagnose an external screen which is black, not detected or has a bad resolution"_s,
            .iconName = u"video-display"_s,
            .request = u"My external screen does not work correctly."_s,
            .steps = {u"list_outputs: connected and enabled screens, modes, scale and position"_s, u"get_system_info: graphic card and driver"_s},
            .fixes = {u"Explain how to enable the screen, choose its native resolution, scale or position in System Settings > Display & Monitor"_s,
                      u"When the screen is not detected: check cable and input of the screen, graphic driver"_s},
            .diagnosticTools = {u"list_outputs"_s, u"get_system_info"_s},
        },
        {
            .name = u"network_problem"_s,
            .title = i18nc("@title scenario", "Internet or Wi-Fi"),
            .description = u"Diagnose and fix Internet or Wi-Fi access"_s,
            .iconName = u"network-wireless"_s,
            .request = u"Internet or Wi-Fi does not work."_s,
            .steps = {u"get_network_status: Wi-Fi enabled, connectivity, addresses, gateway and DNS"_s,
                      u"check_connectivity: DNS resolution and Internet access"_s,
                      u"read_journal with unit NetworkManager.service and system true: recent errors"_s},
            .fixes = {u"set_wifi_enabled when Wi-Fi is disabled (not when disabled by a hardware switch: explain it)"_s,
                      u"reconnect_network for an active connection which doesn't work"_s,
                      u"Captive portal: tell user to open a web browser to log in"_s},
            .diagnosticTools = {u"get_network_status"_s, u"list_connections"_s},
        },
        {
            .name = u"no_sleep"_s,
            .title = i18nc("@title scenario", "Computer Does Not Sleep"),
            .description = u"Find out why computer doesn't sleep or screen doesn't turn off"_s,
            .iconName = u"system-suspend"_s,
            .request = u"My computer does not go to sleep."_s,
            .steps = {u"list_sleep_inhibitors: applications which prevent sleep"_s, u"get_power_status: sleep settings and power profile"_s},
            .fixes = {u"Explain which application blocks sleep and how to close it or change its settings"_s,
                      u"Explain how to change sleep settings in System Settings > Power Management"_s},
            .diagnosticTools = {u"list_sleep_inhibitors"_s, u"get_power_status"_s},
        },
        {
            .name = u"app_crash"_s,
            .title = i18nc("@title scenario", "Application Crashes"),
            .description = u"Find out which application crashed and why, and prepare a bug report"_s,
            .iconName = u"tools-report-bug"_s,
            .request = u"An application crashed."_s,
            .steps = {u"list_recent_crashes: which application crashed, how often"_s,
                      u"get_crash_backtrace with pid of the crash: where it crashed"_s,
                      u"read_journal with executable of the crash: messages before the crash"_s},
            .fixes = {u"Explain the crash in simple words, without guessing"_s, u"prepare_bug_report with pid of the crash when user wants to report it"_s},
            .diagnosticTools = {u"list_recent_crashes"_s},
        },
        {
            .name = u"system_report"_s,
            .title = i18nc("@title scenario", "System Report"),
            .description = u"Summarize the system: versions, hardware, disks"_s,
            .iconName = u"document-properties"_s,
            .request = u"Give me a short report about my system."_s,
            .steps = {u"get_system_info: versions, session, graphic card"_s, u"get_disk_usage: free space"_s},
            .fixes = {u"get_system_report when user wants a full report to attach to a bug report"_s},
            .diagnosticTools = {u"get_system_info"_s, u"get_disk_usage"_s},
        },
    };
}

QString ScenarioPrompts::promptText(const Scenario &scenario, const QString &details, const QString &language)
{
    QString text = scenario.request;
    if (!details.trimmed().isEmpty()) {
        text += u"\nDetails: %1"_s.arg(details.trimmed());
    }
    text += u"\n\nCall these tools to diagnose it, in this order:\n"_s;
    for (qsizetype i = 0; i < scenario.steps.count(); ++i) {
        text += u"%1. %2\n"_s.arg(i + 1).arg(scenario.steps.at(i));
    }
    text += u"\nTypical fixes:\n"_s;
    for (const QString &fix : scenario.fixes) {
        text += u"- %1\n"_s.arg(fix);
    }
    text +=
        u"\nRules: call tools with function calls, never write a tool call or JSON as text in the answer. Diagnose before acting. "
        "Explain the cause in simple words. Propose only one change at a time, then check its effect with a diagnostic tool. "
        "Use names and ids returned by tools, never invent them. Answer in %1."_s.arg(language);
    return text;
}

void ScenarioPrompts::registerPrompts(McpProtocolServerProtocolManager *manager)
{
    const QList<Scenario> list = scenarios();
    for (const Scenario &scenario : list) {
        manager->addPrompt(std::make_unique<ScenarioPrompt>(scenario));
    }
}
