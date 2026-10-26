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

Q_SIGNALS:
    void readyChanged(const QString &error);

private:
    void connectToServer();
    void setReady(const QString &error);
    [[nodiscard]] AudioState state() const;

    QTimer mConnectTimer;
    bool mContextConnected = false;
    bool mConnecting = false;
    bool mReady = false;
};
