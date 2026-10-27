/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class TroubleshooterServerTest : public QObject
{
    Q_OBJECT
public:
    explicit TroubleshooterServerTest(QObject *parent = nullptr);
    ~TroubleshooterServerTest() override = default;

private Q_SLOTS:
    void shouldAnswerInitialize();
    void shouldListToolsWithAnnotations();
    void shouldCallTool_data();
    void shouldCallTool();
    void shouldExitWhenStdinIsClosed();
    void shouldAnswerEveryTool();
    void shouldProvideReportResource();
    void shouldProvideScenarios();
};
