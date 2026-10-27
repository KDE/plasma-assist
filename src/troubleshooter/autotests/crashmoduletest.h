/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class CrashModuleTest : public QObject
{
    Q_OBJECT
public:
    explicit CrashModuleTest(QObject *parent = nullptr);
    ~CrashModuleTest() override = default;

private Q_SLOTS:
    void shouldParseCrashes();
    void shouldParseBacktrace();
    void shouldIgnoreUnknownOutput();
};
