/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class DisplayActionsTest : public QObject
{
    Q_OBJECT
public:
    explicit DisplayActionsTest(QObject *parent = nullptr);
    ~DisplayActionsTest() override = default;

private Q_SLOTS:
    void shouldEnableOutputAtRightOfOthers();
    void shouldChangeResolutionAndRefreshRate();
    void shouldChangeScaleAndPosition();
    void shouldRejectInvalidArguments();
    void shouldRefuseToDisableLastOutput();
    void shouldUndoChange();
};
