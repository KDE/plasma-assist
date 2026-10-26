/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "conversationmodeltest.h"
#include "conversationmodel.h"
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(ConversationModelTest)

namespace
{
QStringList types(const ConversationModel &model)
{
    QStringList result;
    for (int row = 0; row < model.rowCount(); ++row) {
        result.append(model.index(row).data(ConversationModel::TypeRole).toString());
    }
    return result;
}
}

ConversationModelTest::ConversationModelTest(QObject *parent)
    : QObject{parent}
{
}

void ConversationModelTest::shouldBeEmptyByDefault()
{
    const ConversationModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.roleNames().value(ConversationModel::TypeRole), "type"_ba);
}

void ConversationModelTest::shouldLoadMessages()
{
    ConversationModel model;
    // Format of GetMessages()
    model.setMessages(
        uR"([{"id":"1","role":"user","text":"No sound","toolCalls":[]},
             {"id":"2","role":"assistant","text":"Output is muted.",
              "toolCalls":[{"id":"call_1","name":"list_audio_devices","arguments":{"kind":"sink"},"result":"1 output"}]}])"_s);
    QCOMPARE(types(model), (QStringList{u"user"_s, u"tool"_s, u"assistant"_s}));
    const QModelIndex tool = model.index(1);
    QCOMPARE(tool.data(ConversationModel::ToolNameRole).toString(), u"list_audio_devices"_s);
    QCOMPARE(tool.data(ConversationModel::StatusRole).toString(), u"done"_s);
    QCOMPARE(tool.data(ConversationModel::ArgumentsRole).toString(), uR"({"kind":"sink"})"_s);
    QCOMPARE(tool.data(ConversationModel::ResultRole).toString(), u"1 output"_s);
    QCOMPARE(model.index(2).data(ConversationModel::TextRole).toString(), u"Output is muted."_s);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

void ConversationModelTest::shouldFollowAnswer()
{
    ConversationModel model;
    model.addUserMessage(u"No sound"_s);
    model.setAnswer(u"2"_s, u"Out"_s);
    QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
    model.setAnswer(u"2"_s, u"Output is muted."_s);
    QCOMPARE(types(model), (QStringList{u"user"_s, u"assistant"_s}));
    QCOMPARE(dataChanged.count(), 1);
    QCOMPARE(model.index(1).data(ConversationModel::TextRole).toString(), u"Output is muted."_s);
}

void ConversationModelTest::shouldShowAnswerAfterToolCalls()
{
    ConversationModel model;
    model.addUserMessage(u"No sound"_s);
    // Language model can write text before calling tools
    model.setAnswer(u"2"_s, u"Let me check"_s);
    model.startToolCall(u"call_1"_s, u"list_audio_devices"_s, u"{}"_s);
    model.setAnswer(u"2"_s, u"Output is muted."_s);
    QCOMPARE(types(model), (QStringList{u"user"_s, u"tool"_s, u"assistant"_s}));
    QCOMPARE(model.index(2).data(ConversationModel::TextRole).toString(), u"Output is muted."_s);
}

void ConversationModelTest::shouldFinishToolCall()
{
    ConversationModel model;
    model.startToolCall(u"call_1"_s, u"list_audio_devices"_s, u"{}"_s);
    QCOMPARE(model.index(0).data(ConversationModel::StatusRole).toString(), u"running"_s);
    model.finishToolCall(u"call_1"_s, u"1 output"_s, false);
    QCOMPARE(model.index(0).data(ConversationModel::StatusRole).toString(), u"done"_s);
    QCOMPARE(model.index(0).data(ConversationModel::ResultRole).toString(), u"1 output"_s);

    model.startToolCall(u"call_2"_s, u"list_outputs"_s, u"{}"_s);
    model.finishToolCall(u"call_2"_s, u"Error"_s, true);
    QCOMPARE(model.index(1).data(ConversationModel::StatusRole).toString(), u"failed"_s);

    // Unknown call is ignored
    model.finishToolCall(u"unknown"_s, u"Error"_s, true);
    QCOMPARE(model.rowCount(), 2);
}

void ConversationModelTest::shouldAddError()
{
    ConversationModel model;
    model.addError(u"No language model is configured."_s);
    QCOMPARE(types(model), QStringList{u"error"_s});
    QCOMPARE(model.index(0).data(Qt::DisplayRole).toString(), u"No language model is configured."_s);
}

#include "moc_conversationmodeltest.cpp"
