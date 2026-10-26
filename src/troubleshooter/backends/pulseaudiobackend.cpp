/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pulseaudiobackend.h"
#include <PulseAudioQt/Card>
#include <PulseAudioQt/CardPort>
#include <PulseAudioQt/Client>
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Profile>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include <PulseAudioQt/SinkInput>
#include <PulseAudioQt/Source>
#include <QMetaEnum>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace
{
QString availability(PulseAudioQt::Profile::Availability availability)
{
    switch (availability) {
    case PulseAudioQt::Profile::Available:
        return u"available"_s;
    case PulseAudioQt::Profile::Unavailable:
        return u"unavailable"_s;
    case PulseAudioQt::Profile::Unknown:
        break;
    }
    return u"unknown"_s;
}

QString deviceState(PulseAudioQt::Device::State state)
{
    switch (state) {
    case PulseAudioQt::Device::RunningState:
        return u"running"_s;
    case PulseAudioQt::Device::IdleState:
        return u"idle"_s;
    case PulseAudioQt::Device::SuspendedState:
        return u"suspended"_s;
    case PulseAudioQt::Device::InvalidState:
    case PulseAudioQt::Device::UnknownState:
        break;
    }
    return u"unknown"_s;
}

int volumePercent(qint64 volume)
{
    return qRound(volume * 100.0 / PulseAudioQt::normalVolume());
}

template<typename PortType>
AudioPort audioPort(const PortType *port)
{
    return AudioPort{
        .name = port->name(),
        .description = port->description(),
        .type = QString::fromLatin1(QMetaEnum::fromType<PulseAudioQt::Port::Type>().valueToKey(port->type())),
        .availability = availability(port->availability()),
    };
}

AudioDevice audioDevice(const PulseAudioQt::Device *device)
{
    AudioDevice result{
        .name = device->name(),
        .description = device->description(),
        .state = deviceState(device->state()),
        .formFactor = device->formFactor(),
        .ports = {},
        .activePort = {},
        .cardIndex = device->cardIndex(),
        .volumePercent = volumePercent(device->volume()),
        .muted = device->isMuted(),
        .isDefault = device->isDefault(),
        .virtualDevice = device->isVirtualDevice(),
    };
    const QList<PulseAudioQt::Port *> ports = device->ports();
    for (const PulseAudioQt::Port *port : ports) {
        result.ports.append(audioPort(port));
    }
    if (device->activePortIndex() < static_cast<quint32>(ports.count())) {
        result.activePort = ports.at(device->activePortIndex())->name();
    }
    return result;
}

AudioCard audioCard(const PulseAudioQt::Card *card)
{
    AudioCard result{
        .index = card->index(),
        .name = card->name(),
        .description = card->properties().value(u"device.description"_s).toString(),
        .profiles = {},
        .activeProfile = {},
        .ports = {},
    };
    const QList<PulseAudioQt::Profile *> profiles = card->profiles();
    for (const PulseAudioQt::Profile *profile : profiles) {
        result.profiles.append(AudioProfile{
            .name = profile->name(),
            .description = profile->description(),
            .availability = availability(profile->availability()),
            .sinkCount = profile->sinkCount(),
            .sourceCount = profile->sourceCount(),
        });
    }
    if (card->activeProfileIndex() < static_cast<quint32>(profiles.count())) {
        result.activeProfile = profiles.at(card->activeProfileIndex())->name();
    }
    const QList<PulseAudioQt::CardPort *> ports = card->ports();
    for (const PulseAudioQt::CardPort *port : ports) {
        result.ports.append(audioPort(port));
    }
    return result;
}
}

PulseAudioBackend::PulseAudioBackend(QObject *parent)
    : QObject(parent)
{
    mConnectTimer.setSingleShot(true);
    mConnectTimer.setInterval(5s);
    connect(&mConnectTimer, &QTimer::timeout, this, [this]() {
        setReady(u"Audio server (PipeWire or PulseAudio) is not available"_s);
    });
}

PulseAudioBackend::~PulseAudioBackend() = default;

void PulseAudioBackend::fetchState(QObject *context, StateCallback callback)
{
    if (mReady) {
        callback(state(), {});
        return;
    }
    connect(
        this,
        &PulseAudioBackend::readyChanged,
        context,
        [this, callback](const QString &error) {
            callback(error.isEmpty() ? state() : AudioState{}, error);
        },
        Qt::SingleShotConnection);
    connectToServer();
}

void PulseAudioBackend::connectToServer()
{
    PulseAudioQt::Context *context = PulseAudioQt::Context::instance();
    if (!mContextConnected) {
        mContextConnected = true;
        // Context requests server info after all lists: they are filled when server is updated
        connect(context->server(), &PulseAudioQt::Server::updated, this, [this]() {
            if (!mReady) {
                setReady({});
            }
        });
        connect(context, &PulseAudioQt::Context::stateChanged, this, [this, context]() {
            if (mConnecting && (context->state() == PulseAudioQt::Context::State::Failed || context->state() == PulseAudioQt::Context::State::Terminated)) {
                setReady(u"Connection to audio server failed"_s);
            }
        });
    }
    if (mConnecting) {
        return;
    }
    mConnecting = true;
    mConnectTimer.start();
}

void PulseAudioBackend::setReady(const QString &error)
{
    mConnectTimer.stop();
    mConnecting = false;
    // Retry next time when connection failed
    mReady = error.isEmpty();
    Q_EMIT readyChanged(error);
}

AudioState PulseAudioBackend::state() const
{
    const PulseAudioQt::Context *context = PulseAudioQt::Context::instance();
    AudioState result;
    result.serverName = context->server()->isPipeWire() ? u"PipeWire"_s : u"PulseAudio"_s;
    result.serverVersion = context->server()->version();

    const QList<PulseAudioQt::Sink *> sinks = context->sinks();
    for (const PulseAudioQt::Sink *sink : sinks) {
        result.sinks.append(audioDevice(sink));
    }
    const QList<PulseAudioQt::Source *> sources = context->sources();
    for (const PulseAudioQt::Source *source : sources) {
        // Monitors of sinks are not microphones
        if (source->properties().value(u"device.class"_s).toString() == "monitor"_L1) {
            continue;
        }
        result.sources.append(audioDevice(source));
    }
    const QList<PulseAudioQt::Card *> cards = context->cards();
    for (const PulseAudioQt::Card *card : cards) {
        result.cards.append(audioCard(card));
    }
    const QList<PulseAudioQt::SinkInput *> sinkInputs = context->sinkInputs();
    for (const PulseAudioQt::SinkInput *sinkInput : sinkInputs) {
        if (sinkInput->isVirtualStream()) {
            continue;
        }
        QString application = sinkInput->properties().value(u"application.name"_s).toString();
        if (application.isEmpty() && sinkInput->client()) {
            application = sinkInput->client()->name();
        }
        QString device;
        for (const PulseAudioQt::Sink *sink : sinks) {
            if (sink->index() == sinkInput->deviceIndex()) {
                device = sink->name();
                break;
            }
        }
        result.streams.append(AudioStream{
            .index = sinkInput->index(),
            .application = application,
            .device = device,
            .volumePercent = volumePercent(sinkInput->volume()),
            .muted = sinkInput->isMuted(),
            .corked = sinkInput->isCorked(),
        });
    }
    return result;
}

#include "moc_pulseaudiobackend.cpp"
