/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "audiobackend.h"
#include <QObject>
#include <QTimer>

// AudioBackend implemented with PulseAudioQt (PulseAudio or PipeWire)
class PulseAudioBackend : public QObject, public AudioBackend
{
    Q_OBJECT
public:
    explicit PulseAudioBackend(QObject *parent = nullptr);
    ~PulseAudioBackend() override;

    void fetchState(QObject *context, StateCallback callback) override;
    void setDefaultDevice(DeviceKind kind, const QString &name, QObject *context, const ErrorCallback &callback) override;
    void setVolume(DeviceKind kind, const QString &name, int volumePercent, QObject *context, const ErrorCallback &callback) override;
    void setMuted(DeviceKind kind, const QString &name, bool muted, QObject *context, const ErrorCallback &callback) override;
    void setCardProfile(const QString &card, const QString &profile, QObject *context, const ErrorCallback &callback) override;
    void moveStream(quint32 streamIndex, const QString &sink, QObject *context, const ErrorCallback &callback) override;

Q_SIGNALS:
    void readyChanged(const QString &error);

private:
    // Calls \a function when connected to audio server, or \a callback with error
    void whenReady(QObject *context, const ErrorCallback &callback, const std::function<QString()> &function);
    void connectToServer();
    void setReady(const QString &error);
    [[nodiscard]] AudioState state() const;

    QTimer mConnectTimer;
    bool mContextConnected = false;
    bool mConnecting = false;
    bool mReady = false;
};
