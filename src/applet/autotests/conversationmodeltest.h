/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class ConversationModelTest : public QObject
{
    Q_OBJECT
public:
    explicit ConversationModelTest(QObject *parent = nullptr);
    ~ConversationModelTest() override = default;

private Q_SLOTS:
    void shouldBeEmptyByDefault();
    void shouldLoadMessages();
    void shouldFollowAnswer();
    void shouldShowAnswerAfterToolCalls();
    void shouldFinishToolCall();
    void shouldAddError();
    void shouldFollowConfirmation();
};
