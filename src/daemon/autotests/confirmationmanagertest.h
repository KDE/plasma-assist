/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class ConfirmationManagerTest : public QObject
{
    Q_OBJECT
public:
    explicit ConfirmationManagerTest(QObject *parent = nullptr);
    ~ConfirmationManagerTest() override = default;

private Q_SLOTS:
    void initTestCase();
    void init();
    void shouldRefuseWhenActionsAreDisabled();
    void shouldAskUser();
    void shouldDenyWhenUserRefuses();
    void shouldRejectAnswerOfAnotherClient();
    void shouldDenyAfterTimeout();
    void shouldDenyWithoutClient();
    void shouldAlwaysAllowModifyTools();
    void shouldNeverAlwaysAllowDestructiveTools();
    void shouldCreateSummary();
    void shouldAskToKeepChange();
};
