/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class AssistantTest : public QObject
{
    Q_OBJECT
public:
    explicit AssistantTest(QObject *parent = nullptr);
    ~AssistantTest() override = default;

private Q_SLOTS:
    void shouldHaveDefaultStatus();
    void initTestCase();
    void shouldCreateConversation();
    void shouldKeepConversations();
    void shouldRejectMessageForUnknownConversation();
    void shouldReportErrorWithoutLanguageModel();
    void shouldStartTroubleshooterOnDemand();
    void shouldCallToolWithoutLanguageModel();
    void shouldReturnErrorForUnknownTool();
    void shouldRejectInvalidArguments();
    void shouldRefuseActionWithoutLanguageModel();
};
