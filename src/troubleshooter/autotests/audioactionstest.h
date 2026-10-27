/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class AudioActionsTest : public QObject
{
    Q_OBJECT
public:
    explicit AudioActionsTest(QObject *parent = nullptr);
    ~AudioActionsTest() override = default;

private Q_SLOTS:
    void shouldSetDefaultDevice();
    void shouldSetVolumeOfDefaultDevice();
    void shouldRejectUnknownDevice();
    void shouldChangeCardProfile();
    void shouldMoveStream();
    void shouldRestorePseudonyms();
};
