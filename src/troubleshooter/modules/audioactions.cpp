/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "audioactions.h"
#include "backends/audiobackend.h"
#include "core/toolhelpers.h"
#include <KLocalizedString>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerTool>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr int maximumVolumePercent = 100;

AudioBackend::DeviceKind deviceKind(const QJsonObject &arguments)
{
    return arguments.value("kind"_L1).toString() == "source"_L1 ? AudioBackend::DeviceKind::Source : AudioBackend::DeviceKind::Sink;
}

const QList<AudioDevice> &devices(const AudioState &state, AudioBackend::DeviceKind kind)
{
    return kind == AudioBackend::DeviceKind::Sink ? state.sinks : state.sources;
}

const AudioDevice *findDevice(const AudioState &state, AudioBackend::DeviceKind kind, const QString &name)
{
    for (const AudioDevice &device : devices(state, kind)) {
        if (device.name == name) {
            return &device;
        }
    }
    return nullptr;
}

const AudioDevice *defaultDevice(const AudioState &state, AudioBackend::DeviceKind kind)
{
    for (const AudioDevice &device : devices(state, kind)) {
        if (device.isDefault) {
            return &device;
        }
    }
    return nullptr;
}

// Device of arguments, or default device when argument "device" is missing
const AudioDevice *argumentDevice(const AudioState &state, const QJsonObject &arguments)
{
    const AudioBackend::DeviceKind kind = deviceKind(arguments);
    const QString name = arguments.value("device"_L1).toString();
    return name.isEmpty() ? defaultDevice(state, kind) : findDevice(state, kind, name);
}

QString description(const AudioState &state, const QString &name)
{
    for (const QList<AudioDevice> *list : {&state.sinks, &state.sources}) {
        for (const AudioDevice &device : *list) {
            if (device.name == name) {
                return device.description;
            }
        }
    }
    return name;
}

QJsonObject kindProperty()
{
    return McpProtocolServerTool::schemaEnumProperty({u"sink"_s, u"source"_s}, u"sink for an output, source for an input (default: sink)"_s);
}

QJsonObject deviceProperty(const QString &description)
{
    return QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, description}};
}

// Last state read: used to check arguments and to write summaries with descriptions of devices
using StateCache = std::shared_ptr<AudioState>;

// Reads audio state and converts it with \a convert (it returns an error when arguments are invalid)
std::function<void(const QJsonObject &, QObject *, const Action::StateCallback &)>
readState(AudioBackend *backend,
          const StateCache &cache,
          const std::function<QJsonObject(const AudioState &state, const QJsonObject &arguments, QString *error)> &convert)
{
    return [backend, cache, convert](const QJsonObject &arguments, QObject *context, const Action::StateCallback &callback) {
        backend->fetchState(context, [cache, convert, arguments, callback](const AudioState &state, const QString &error) {
            if (!error.isEmpty()) {
                callback({}, error);
                return;
            }
            *cache = state;
            QString convertError;
            const QJsonObject result = convert(state, arguments, &convertError);
            callback(convertError.isEmpty() ? result : QJsonObject(), convertError);
        });
    };
}

Action defaultDeviceAction(AudioBackend *backend)
{
    auto cache = std::make_shared<AudioState>();
    return Action{
        .readState = readState(backend,
                               cache,
                               [](const AudioState &state, const QJsonObject &arguments, QString *) {
                                   const AudioDevice *device = defaultDevice(state, deviceKind(arguments));
                                   return QJsonObject{{u"device"_s, device ? device->name : QString()}};
                               }),
        .targetState =
            [cache](const QJsonObject &arguments, const QJsonObject &, QString *error) {
                const QString name = arguments.value("device"_L1).toString();
                if (!findDevice(*cache, deviceKind(arguments), name)) {
                    *error = u"Unknown audio device: %1 (use names of list_audio_devices)"_s.arg(name);
                    return QJsonObject();
                }
                return QJsonObject{{u"device"_s, name}};
            },
        .applyState =
            [backend](const QJsonObject &arguments, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                backend->setDefaultDevice(deviceKind(arguments), state.value("device"_L1).toString(), context, callback);
            },
        .summary =
            [cache](const QJsonObject &before, const QJsonObject &after) {
                return i18nc("@info change",
                             "Default audio device: %1 → %2",
                             description(*cache, before.value("device"_L1).toString()),
                             description(*cache, after.value("device"_L1).toString()));
            },
    };
}

Action volumeAction(AudioBackend *backend)
{
    auto cache = std::make_shared<AudioState>();
    return Action{
        .readState = readState(backend,
                               cache,
                               [](const AudioState &state, const QJsonObject &arguments, QString *error) {
                                   const AudioDevice *device = argumentDevice(state, arguments);
                                   if (!device) {
                                       *error = u"Unknown audio device: %1"_s.arg(arguments.value("device"_L1).toString());
                                       return QJsonObject();
                                   }
                                   return QJsonObject{{u"device"_s, device->name}, {u"volumePercent"_s, device->volumePercent}};
                               }),
        .targetState =
            [](const QJsonObject &arguments, const QJsonObject &current, QString *error) {
                const int volume = static_cast<int>(ToolHelpers::integerArgument(arguments.value("volume_percent"_L1)).value_or(-1));
                if (volume < 0 || volume > maximumVolumePercent) {
                    *error = u"volume_percent must be between 0 and %1"_s.arg(maximumVolumePercent);
                    return QJsonObject();
                }
                QJsonObject target = current;
                target.insert(u"volumePercent"_s, volume);
                return target;
            },
        .applyState =
            [backend](const QJsonObject &arguments, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                backend->setVolume(deviceKind(arguments), state.value("device"_L1).toString(), state.value("volumePercent"_L1).toInt(), context, callback);
            },
        .summary =
            [cache](const QJsonObject &before, const QJsonObject &after) {
                return i18nc("@info change",
                             "Volume of %1: %2% → %3%",
                             description(*cache, after.value("device"_L1).toString()),
                             before.value("volumePercent"_L1).toInt(),
                             after.value("volumePercent"_L1).toInt());
            },
    };
}

Action muteAction(AudioBackend *backend)
{
    auto cache = std::make_shared<AudioState>();
    return Action{
        .readState = readState(backend,
                               cache,
                               [](const AudioState &state, const QJsonObject &arguments, QString *error) {
                                   const AudioDevice *device = argumentDevice(state, arguments);
                                   if (!device) {
                                       *error = u"Unknown audio device: %1"_s.arg(arguments.value("device"_L1).toString());
                                       return QJsonObject();
                                   }
                                   return QJsonObject{{u"device"_s, device->name}, {u"muted"_s, device->muted}};
                               }),
        .targetState =
            [](const QJsonObject &arguments, const QJsonObject &current, QString *error) {
                const std::optional<bool> muted = ToolHelpers::booleanArgument(arguments.value("muted"_L1));
                if (!muted) {
                    *error = u"muted must be true or false"_s;
                    return QJsonObject();
                }
                QJsonObject target = current;
                target.insert(u"muted"_s, *muted);
                return target;
            },
        .applyState =
            [backend](const QJsonObject &arguments, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                backend->setMuted(deviceKind(arguments), state.value("device"_L1).toString(), state.value("muted"_L1).toBool(), context, callback);
            },
        .summary =
            [cache](const QJsonObject &, const QJsonObject &after) {
                const QString device = description(*cache, after.value("device"_L1).toString());
                return after.value("muted"_L1).toBool() ? i18nc("@info change", "%1 muted", device) : i18nc("@info change", "%1 unmuted", device);
            },
    };
}

Action cardProfileAction(AudioBackend *backend)
{
    auto cache = std::make_shared<AudioState>();
    return Action{
        .readState = readState(backend,
                               cache,
                               [](const AudioState &state, const QJsonObject &arguments, QString *error) {
                                   const QString name = arguments.value("card"_L1).toString();
                                   for (const AudioCard &card : state.cards) {
                                       if (card.name == name) {
                                           return QJsonObject{{u"card"_s, card.name}, {u"profile"_s, card.activeProfile}};
                                       }
                                   }
                                   *error = u"Unknown audio card: %1 (use names of list_audio_cards)"_s.arg(name);
                                   return QJsonObject();
                               }),
        .targetState =
            [cache](const QJsonObject &arguments, const QJsonObject &current, QString *error) {
                const QString profile = arguments.value("profile"_L1).toString();
                for (const AudioCard &card : std::as_const(cache->cards)) {
                    if (card.name != current.value("card"_L1).toString()) {
                        continue;
                    }
                    const bool known = std::ranges::any_of(card.profiles, [&profile](const AudioProfile &cardProfile) {
                        return cardProfile.name == profile;
                    });
                    if (!known) {
                        *error = u"Unknown profile %1 for card %2"_s.arg(profile, card.name);
                        return QJsonObject();
                    }
                }
                QJsonObject target = current;
                target.insert(u"profile"_s, profile);
                return target;
            },
        .applyState =
            [backend](const QJsonObject &, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                backend->setCardProfile(state.value("card"_L1).toString(), state.value("profile"_L1).toString(), context, callback);
            },
        .summary =
            [cache](const QJsonObject &before, const QJsonObject &after) {
                QString card = after.value("card"_L1).toString();
                for (const AudioCard &audioCard : std::as_const(cache->cards)) {
                    if (audioCard.name == card && !audioCard.description.isEmpty()) {
                        card = audioCard.description;
                    }
                }
                return i18nc("@info change", "Profile of %1: %2 → %3", card, before.value("profile"_L1).toString(), after.value("profile"_L1).toString());
            },
    };
}

Action moveStreamAction(AudioBackend *backend)
{
    auto cache = std::make_shared<AudioState>();
    return Action{
        .readState = readState(backend,
                               cache,
                               [](const AudioState &state, const QJsonObject &arguments, QString *error) {
                                   const qint64 index = ToolHelpers::integerArgument(arguments.value("stream"_L1)).value_or(-1);
                                   for (const AudioStream &stream : state.streams) {
                                       if (static_cast<qint64>(stream.index) == index) {
                                           return QJsonObject{{u"stream"_s, index}, {u"device"_s, stream.device}};
                                       }
                                   }
                                   *error = u"Unknown audio stream: %1 (use indexes of list_audio_streams)"_s.arg(index);
                                   return QJsonObject();
                               }),
        .targetState =
            [cache](const QJsonObject &arguments, const QJsonObject &current, QString *error) {
                const QString name = arguments.value("device"_L1).toString();
                if (!findDevice(*cache, AudioBackend::DeviceKind::Sink, name)) {
                    *error = u"Unknown audio output: %1"_s.arg(name);
                    return QJsonObject();
                }
                QJsonObject target = current;
                target.insert(u"device"_s, name);
                return target;
            },
        .applyState =
            [backend](const QJsonObject &, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                backend->moveStream(state.value("stream"_L1).toInteger(), state.value("device"_L1).toString(), context, callback);
            },
        .summary =
            [cache](const QJsonObject &before, const QJsonObject &after) {
                QString application;
                for (const AudioStream &stream : std::as_const(cache->streams)) {
                    if (static_cast<qint64>(stream.index) == after.value("stream"_L1).toInteger()) {
                        application = stream.application;
                    }
                }
                return i18nc("@info change",
                             "Sound of %1: %2 → %3",
                             application,
                             description(*cache, before.value("device"_L1).toString()),
                             description(*cache, after.value("device"_L1).toString()));
            },
    };
}
}

QList<std::pair<McpProtocolTool, Action>> AudioActions::actions(AudioBackend *backend)
{
    const ToolHelpers::ToolGroup group{u"audio"_s, i18nc("@title tools group", "Audio")};
    const auto tool = [&group](const QString &name,
                               const QString &title,
                               const QString &description,
                               const QMap<QString, QJsonObject> &properties,
                               const QStringList &required) {
        return ToolHelpers::createTool(name, title, description, ToolHelpers::ToolKind::ReversibleAction, group, properties, required);
    };
    return {
        {tool(u"set_default_audio_device"_s,
              i18nc("@title tool", "Change Default Audio Device"),
              u"Change the default output (sink) or input (source), e.g. to use plugged headphones. Applications which use default device follow."_s,
              {{u"kind"_s, kindProperty()}, {u"device"_s, deviceProperty(u"Name of device given by list_audio_devices"_s)}},
              {u"device"_s}),
         defaultDeviceAction(backend)},
        {tool(u"set_audio_volume"_s,
              i18nc("@title tool", "Change Volume"),
              u"Change volume of an output or input device (default device when device is not given)."_s,
              {{u"kind"_s, kindProperty()},
               {u"device"_s, deviceProperty(u"Name of device given by list_audio_devices (default: default device)"_s)},
               {u"volume_percent"_s, QJsonObject{{u"type"_s, u"integer"_s}, {u"description"_s, u"Volume between 0 and %1"_s.arg(maximumVolumePercent)}}}},
              {u"volume_percent"_s}),
         volumeAction(backend)},
        {tool(u"set_audio_mute"_s,
              i18nc("@title tool", "Mute or Unmute"),
              u"Mute or unmute an output or input device (default device when device is not given)."_s,
              {{u"kind"_s, kindProperty()},
               {u"device"_s, deviceProperty(u"Name of device given by list_audio_devices (default: default device)"_s)},
               {u"muted"_s, QJsonObject{{u"type"_s, u"boolean"_s}, {u"description"_s, u"true to mute, false to unmute"_s}}}},
              {u"muted"_s}),
         muteAction(backend)},
        {tool(u"set_card_profile"_s,
              i18nc("@title tool", "Change Audio Card Profile"),
              u"Change profile of an audio card: analog or HDMI output, Bluetooth high quality (A2DP) or headset with microphone (HFP)."_s,
              {{u"card"_s, deviceProperty(u"Name of card given by list_audio_cards"_s)},
               {u"profile"_s, deviceProperty(u"Name of profile given by list_audio_cards"_s)}},
              {u"card"_s, u"profile"_s}),
         cardProfileAction(backend)},
        {tool(u"move_audio_stream"_s,
              i18nc("@title tool", "Move Application Sound"),
              u"Move sound of an application to another output."_s,
              {{u"stream"_s, QJsonObject{{u"type"_s, u"integer"_s}, {u"description"_s, u"Index of stream given by list_audio_streams"_s}}},
               {u"device"_s, deviceProperty(u"Name of output given by list_audio_devices"_s)}},
              {u"stream"_s, u"device"_s}),
         moveStreamAction(backend)},
    };
}

void AudioActions::registerTools(McpProtocolServerProtocolManager *manager, AudioBackend *backend, ActionRunner *runner)
{
    const QList<std::pair<McpProtocolTool, Action>> list = actions(backend);
    for (const auto &[definition, action] : list) {
        runner->addActionTool(manager, definition, action);
    }
}
