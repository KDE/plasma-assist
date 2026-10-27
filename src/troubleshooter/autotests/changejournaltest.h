/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class ChangeJournalTest : public QObject
{
    Q_OBJECT
public:
    explicit ChangeJournalTest(QObject *parent = nullptr);
    ~ChangeJournalTest() override = default;

private Q_SLOTS:
    void shouldBeEmptyByDefault();
    void shouldAddChanges();
    void shouldKeepChangesAfterRestart();
    void shouldLimitNumberOfChanges();
};
