/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class ActionRunnerTest : public QObject
{
    Q_OBJECT
public:
    explicit ActionRunnerTest(QObject *parent = nullptr);
    ~ActionRunnerTest() override = default;

private Q_SLOTS:
    void shouldApplyAndUndo();
    void shouldNotChangeSameState();
    void shouldRejectInvalidArguments();
    void shouldRefuseUndoWhenStateChanged();
    void shouldReportChangeNotApplied();
    void shouldWaitForAsynchronousChange();
    void shouldRecordActionWithoutState();
};
