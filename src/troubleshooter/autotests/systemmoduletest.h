/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class SystemModuleTest : public QObject
{
    Q_OBJECT
public:
    explicit SystemModuleTest(QObject *parent = nullptr);
    ~SystemModuleTest() override = default;

private Q_SLOTS:
    void shouldParseOsRelease();
    void shouldIgnoreInvalidOsReleaseLines();
    void shouldDetectLowSpace_data();
    void shouldDetectLowSpace();
    void shouldMergeVolumesByDevice();
    void shouldReturnSystemInfo();
};
