/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class AudioModuleTest : public QObject
{
    Q_OBJECT
public:
    explicit AudioModuleTest(QObject *parent = nullptr);
    ~AudioModuleTest() override = default;

private Q_SLOTS:
    void shouldHaveNoHintWhenEverythingIsFine();
    void shouldDetectNoOutput();
    void shouldDetectMutedDefaultOutput();
    void shouldDetectHeadphonesOnAnotherOutput();
    void shouldDetectHeadphonesOnInactivePort();
    void shouldDetectUnpluggedActivePort();
    void shouldDetectMutedStream();
    void shouldFilterDevicesByKind();
    void shouldConvertCards();
    void shouldConvertStreams();
};
