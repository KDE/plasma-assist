/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class DisplayModuleTest : public QObject
{
    Q_OBJECT
public:
    explicit DisplayModuleTest(QObject *parent = nullptr);
    ~DisplayModuleTest() override = default;

private Q_SLOTS:
    void shouldHaveNoHintWhenEverythingIsFine();
    void shouldDetectConnectedButDisabledOutput();
    void shouldDetectNotNativeMode();
    void shouldDetectRotation();
    void shouldDetectNoEnabledOutput();
    void shouldListDisconnectedOutputsByName();
    void shouldGroupModesBySize();
    void shouldLimitModes();
};
