/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class LogModuleTest : public QObject
{
    Q_OBJECT
public:
    explicit LogModuleTest(QObject *parent = nullptr);
    ~LogModuleTest() override = default;

private Q_SLOTS:
    void shouldBuildArguments();
    void shouldRejectInvalidArguments();
    void shouldParseEntries();
};
