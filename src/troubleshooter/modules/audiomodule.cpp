/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "audiomodule.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <QJsonArray>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
QJsonArray portsToJson(const QList<AudioPort> &ports)
{
    QJsonArray result;
    for (const AudioPort &port : ports) {
        result.append(QJsonObject{
            {u"name"_s, port.name},
            {u"description"_s, port.description},
            {u"type"_s, port.type},
            {u"availability"_s, port.availability},
        });
    }
    return result;
}

QJsonArray devicesToArray(const QList<AudioDevice> &devices)
{
    QJsonArray result;
    for (const AudioDevice &device : devices) {
        result.append(QJsonObject{
            {u"name"_s, device.name},
            {u"description"_s, device.description},
            {u"default"_s, device.isDefault},
            {u"muted"_s, device.muted},
            {u"volumePercent"_s, device.volumePercent},
            {u"state"_s, device.state},
            {u"formFactor"_s, device.formFactor},
            {u"activePort"_s, device.activePort},
            {u"ports"_s, portsToJson(device.ports)},
            {u"cardIndex"_s, static_cast<qint64>(device.cardIndex)},
            {u"virtual"_s, device.virtualDevice},
        });
    }
    return result;
}

const AudioDevice *defaultDevice(const QList<AudioDevice> &devices)
{
    for (const AudioDevice &device : devices) {
        if (device.isDefault) {
            return &device;
        }
    }
    return nullptr;
}

bool isHeadphonesPort(const AudioPort &port)
{
    return port.type == "Headphones"_L1 || port.type == "Headset"_L1;
}

bool isHeadphonesDevice(const AudioDevice &device)
{
    return device.formFactor == "headphone"_L1 || device.formFactor == "headset"_L1;
}

void finishWithState(McpProtocolServerToolCall *call,
                     AudioBackend *backend,
                     const std::function<void(McpProtocolServerToolCall *call, const AudioState &state)> &handler)
{
    backend->fetchState(call, [call, handler](const AudioState &state, const QString &error) {
        if (!error.isEmpty()) {
            ToolHelpers::finishWithError(call, error);
            return;
        }
        handler(call, state);
    });
}
}

QJsonObject AudioModule::devicesToJson(const AudioState &state, const QString &kind)
{
    QJsonObject result{
        {u"server"_s, QJsonObject{{u"name"_s, state.serverName}, {u"version"_s, state.serverVersion}}},
        {u"hints"_s, QJsonArray::fromStringList(hints(state))},
    };
    if (kind != "source"_L1) {
        result.insert(u"sinks"_s, devicesToArray(state.sinks));
    }
    if (kind != "sink"_L1) {
        result.insert(u"sources"_s, devicesToArray(state.sources));
    }
    return result;
}

QJsonObject AudioModule::cardsToJson(const AudioState &state)
{
    QJsonArray cards;
    for (const AudioCard &card : state.cards) {
        QJsonArray profiles;
        for (const AudioProfile &profile : card.profiles) {
            profiles.append(QJsonObject{
                {u"name"_s, profile.name},
                {u"description"_s, profile.description},
                {u"availability"_s, profile.availability},
                {u"sinkCount"_s, static_cast<qint64>(profile.sinkCount)},
                {u"sourceCount"_s, static_cast<qint64>(profile.sourceCount)},
            });
        }
        cards.append(QJsonObject{
            {u"index"_s, static_cast<qint64>(card.index)},
            {u"name"_s, card.name},
            {u"description"_s, card.description},
            {u"activeProfile"_s, card.activeProfile},
            {u"profiles"_s, profiles},
            {u"ports"_s, portsToJson(card.ports)},
        });
    }
    return QJsonObject{{u"cards"_s, cards}};
}

QJsonObject AudioModule::streamsToJson(const AudioState &state)
{
    QJsonArray streams;
    for (const AudioStream &stream : state.streams) {
        streams.append(QJsonObject{
            {u"index"_s, static_cast<qint64>(stream.index)},
            {u"application"_s, stream.application},
            {u"device"_s, stream.device},
            {u"volumePercent"_s, stream.volumePercent},
            {u"muted"_s, stream.muted},
            {u"paused"_s, stream.corked},
        });
    }
    return QJsonObject{{u"streams"_s, streams}};
}

QStringList AudioModule::hints(const AudioState &state)
{
    QStringList result;
    if (state.sinks.isEmpty()) {
        result.append(u"No output device found: check that the audio card is detected and its profile is not off"_s);
        return result;
    }
    const AudioDevice *sink = defaultDevice(state.sinks);
    if (!sink) {
        result.append(u"There is no default output device"_s);
        return result;
    }
    if (sink->muted) {
        result.append(u"Default output \"%1\" is muted"_s.arg(sink->description));
    }
    if (sink->volumePercent == 0) {
        result.append(u"Default output \"%1\" volume is 0%"_s.arg(sink->description));
    }
    for (const AudioPort &port : sink->ports) {
        if (port.name == sink->activePort && port.availability == "unavailable"_L1) {
            result.append(u"Active port \"%1\" of default output \"%2\" is unplugged"_s.arg(port.description, sink->description));
        } else if (port.name != sink->activePort && isHeadphonesPort(port) && port.availability == "available"_L1) {
            result.append(u"Headphones are plugged on port \"%1\" of default output \"%2\" but active port is \"%3\""_s.arg(port.description,
                                                                                                                            sink->description,
                                                                                                                            sink->activePort));
        }
    }
    for (const AudioDevice &device : state.sinks) {
        if (device.isDefault) {
            continue;
        }
        const bool headphonesPortAvailable = std::any_of(device.ports.cbegin(), device.ports.cend(), [](const AudioPort &port) {
            return isHeadphonesPort(port) && port.availability == "available"_L1;
        });
        if (headphonesPortAvailable || isHeadphonesDevice(device)) {
            result.append(u"Headphones are available on output \"%1\" but default output is \"%2\""_s.arg(device.description, sink->description));
        }
    }
    for (const AudioStream &stream : state.streams) {
        if (stream.muted) {
            result.append(u"Sound of application \"%1\" is muted"_s.arg(stream.application));
        }
    }
    return result;
}

void AudioModule::registerTools(McpProtocolServerProtocolManager *manager, AudioBackend *backend)
{
    const ToolHelpers::ToolGroup group{u"audio"_s, i18nc("@title tools group", "Audio")};

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(
            u"list_audio_devices"_s,
            i18nc("@title tool", "Audio Devices"),
            u"List audio outputs (sinks) and inputs (sources): default device, muted state, volume, active port and ports availability "
            "(plugged or not). Result contains hints about obvious problems. Use it first when there is no sound or the microphone doesn't work."_s,
            ToolHelpers::ToolKind::Diagnostic,
            group,
            {{u"kind"_s, McpProtocolServerTool::schemaEnumProperty({u"sink"_s, u"source"_s, u"all"_s}, u"Devices to list (default: all)"_s)}}),
        [backend](McpProtocolServerToolCall *call) {
            const QString kind = call->arguments().value(u"kind"_s).toString(u"all"_s);
            finishWithState(call, backend, [kind](McpProtocolServerToolCall *call, const AudioState &state) {
                const AudioDevice *sink = defaultDevice(state.sinks);
                const QStringList problems = hints(state);
                QString summary = u"%1 outputs, %2 inputs, default output: %3"_s.arg(state.sinks.count())
                                      .arg(state.sources.count())
                                      .arg(sink ? sink->description : u"none"_s);
                if (!problems.isEmpty()) {
                    summary += u". "_s + problems.join(u". "_s);
                }
                ToolHelpers::finishWithJson(call, devicesToJson(state, kind), summary);
            });
        });

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(u"list_audio_cards"_s,
                                i18nc("@title tool", "Audio Cards"),
                                u"List audio cards with their available profiles and active profile (analog, HDMI, Bluetooth A2DP or hands-free…). "
                                "Use it when an output is missing or a Bluetooth headset has a bad sound quality."_s,
                                ToolHelpers::ToolKind::Diagnostic,
                                group),
        [backend](McpProtocolServerToolCall *call) {
            finishWithState(call, backend, [](McpProtocolServerToolCall *call, const AudioState &state) {
                ToolHelpers::finishWithJson(call, cardsToJson(state), u"%1 audio cards"_s.arg(state.cards.count()));
            });
        });

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"list_audio_streams"_s,
                                                         i18nc("@title tool", "Audio Streams"),
                                                         u"List applications which play sound, with the output they use, their volume and muted state. "
                                                         "Use it when only one application has no sound."_s,
                                                         ToolHelpers::ToolKind::Diagnostic,
                                                         group),
                                 [backend](McpProtocolServerToolCall *call) {
                                     finishWithState(call, backend, [](McpProtocolServerToolCall *call, const AudioState &state) {
                                         ToolHelpers::finishWithJson(call, streamsToJson(state), u"%1 audio streams"_s.arg(state.streams.count()));
                                     });
                                 });
}
