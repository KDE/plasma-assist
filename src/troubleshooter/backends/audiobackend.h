/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <functional>

class QObject;

// Snapshot of audio server state: modules don't depend on PulseAudioQt (tests use fake states)
struct AudioPort {
    QString name;
    QString description;
    QString type; // Headphones, HDMI, Speaker…
    QString availability; // available, unavailable, unknown
};

struct AudioDevice {
    QString name;
    QString description;
    QString state; // running, idle, suspended, unknown
    QString formFactor;
    QList<AudioPort> ports;
    QString activePort;
    quint32 cardIndex = 0;
    int volumePercent = 0;
    bool muted = false;
    bool isDefault = false;
    bool virtualDevice = false;
};

struct AudioProfile {
    QString name;
    QString description;
    QString availability;
    quint32 sinkCount = 0;
    quint32 sourceCount = 0;
};

struct AudioCard {
    quint32 index = 0;
    QString name;
    QString description;
    QList<AudioProfile> profiles;
    QString activeProfile;
    QList<AudioPort> ports;
};

struct AudioStream {
    quint32 index = 0;
    QString application;
    QString device; // sink name
    int volumePercent = 0;
    bool muted = false;
    bool corked = false; // paused
};

struct AudioState {
    QString serverName; // PipeWire or PulseAudio
    QString serverVersion;
    QList<AudioDevice> sinks;
    QList<AudioDevice> sources;
    QList<AudioCard> cards;
    QList<AudioStream> streams;
};

class AudioBackend
{
public:
    // Error is empty on success
    using StateCallback = std::function<void(const AudioState &state, const QString &error)>;
    using ErrorCallback = std::function<void(const QString &error)>;
    enum class DeviceKind : uint8_t {
        Sink,
        Source,
    };

    AudioBackend();
    virtual ~AudioBackend();

    // Asynchronous (connection to audio server). Callback is not called when \a context is deleted
    virtual void fetchState(QObject *context, StateCallback callback) = 0;

    // Changes are asynchronous: callback is called when request is sent, read state to know when it's applied
    virtual void setDefaultDevice(DeviceKind kind, const QString &name, QObject *context, const ErrorCallback &callback) = 0;
    virtual void setVolume(DeviceKind kind, const QString &name, int volumePercent, QObject *context, const ErrorCallback &callback) = 0;
    virtual void setMuted(DeviceKind kind, const QString &name, bool muted, QObject *context, const ErrorCallback &callback) = 0;
    virtual void setCardProfile(const QString &card, const QString &profile, QObject *context, const ErrorCallback &callback) = 0;
    // Moves stream \a streamIndex of an application to sink \a sink
    virtual void moveStream(quint32 streamIndex, const QString &sink, QObject *context, const ErrorCallback &callback) = 0;

private:
    Q_DISABLE_COPY_MOVE(AudioBackend)
};
