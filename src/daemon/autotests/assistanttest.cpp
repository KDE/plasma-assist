/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "assistanttest.h"
#include "assistant.h"
#include "conversationmanager.h"
#include "troubleshooterclient.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(AssistantTest)

namespace
{
QJsonObject statusObject(const QString &status)
{
    return QJsonDocument::fromJson(status.toUtf8()).object();
}

// Waits for result of Assistant::callTool()
QJsonObject callTool(Assistant &assistant, const QString &name, const QString &arguments)
{
    QJsonObject result;
    bool finished = false;
    assistant.callTool(name, arguments, &assistant, [&result, &finished](const QString &answer) {
        result = QJsonDocument::fromJson(answer.toUtf8()).object();
        finished = true;
    });
    if (!QTest::qWaitFor(
            [&finished]() {
                return finished;
            },
            10000)) {
        return {};
    }
    return result;
}
}

AssistantTest::AssistantTest(QObject *parent)
    : QObject{parent}
{
}

void AssistantTest::initTestCase()
{
    // Troubleshooter (built next to test) is a QGuiApplication: it must start without display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Conversations database and language model configuration of tests: no language model is configured
    QStandardPaths::setTestModeEnabled(true);
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).removeRecursively();
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/autogeneratetextinstances"_s);
}

void AssistantTest::shouldHaveDefaultStatus()
{
    const Assistant assistant;
    const QJsonObject status = statusObject(assistant.Status());
    QVERIFY(!status.value("version"_L1).toString().isEmpty());
    QVERIFY(!status.value("ready"_L1).toBool());
    QCOMPARE(status.value("languageModel"_L1).toObject().value("status"_L1).toString(), u"notloaded"_s);
}

void AssistantTest::shouldCreateConversation()
{
    Assistant assistant;
    QSignalSpy conversationsChanged(&assistant, &Assistant::ConversationsChanged);
    const QString first = assistant.NewConversation();
    const QString second = assistant.NewConversation();
    QVERIFY(!first.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(conversationsChanged.count(), 2);

    QStringList identifiers;
    const QJsonArray conversations = QJsonDocument::fromJson(assistant.ListConversations().toUtf8()).array();
    for (const QJsonValue &conversation : conversations) {
        identifiers.append(conversation["id"_L1].toString());
    }
    QVERIFY(identifiers.contains(first));
    QVERIFY(identifiers.contains(second));
    QCOMPARE(QJsonDocument::fromJson(assistant.GetMessages(first).toUtf8()).array().count(), 0);
}

void AssistantTest::shouldClearConversations()
{
    Assistant assistant;
    (void)assistant.NewConversation();
    QSignalSpy conversationsChanged(&assistant, &Assistant::ConversationsChanged);
    assistant.ClearConversations();
    QCOMPARE(conversationsChanged.count(), 1);
    QVERIFY(QJsonDocument::fromJson(assistant.ListConversations().toUtf8()).array().isEmpty());
}

void AssistantTest::shouldKeepConversations()
{
    QString identifier;
    {
        Assistant assistant;
        identifier = assistant.NewConversation();
    }
    Assistant assistant;
    QVERIFY(assistant.conversationManager()->hasConversation(identifier.toLatin1()));
}

void AssistantTest::shouldRejectMessageForUnknownConversation()
{
    Assistant assistant;
    QVERIFY(!assistant.conversationManager()->sendMessage("unknown", u"No sound"_s).isEmpty());
    const QByteArray conversation = assistant.NewConversation().toLatin1();
    QVERIFY(!assistant.conversationManager()->sendMessage(conversation, u"  "_s).isEmpty());
    QCOMPARE(assistant.conversationManager()->engineStatus(), ConversationManager::EngineStatus::NotLoaded);
}

void AssistantTest::shouldReportErrorWithoutLanguageModel()
{
    Assistant assistant;
    QSignalSpy conversationError(&assistant, &Assistant::ConversationError);
    const QString conversation = assistant.NewConversation();
    assistant.SendMessage(conversation, u"No sound"_s);
    QTRY_COMPARE_WITH_TIMEOUT(conversationError.count(), 1, 10000);
    QCOMPARE(conversationError.at(0).at(0).toString(), conversation);
    QVERIFY(!conversationError.at(0).at(1).toString().isEmpty());

    const QJsonObject languageModel = statusObject(assistant.Status()).value("languageModel"_L1).toObject();
    QCOMPARE(languageModel.value("status"_L1).toString(), u"error"_s);
    QVERIFY(!languageModel.value("error"_L1).toString().isEmpty());
    QVERIFY(!assistant.conversationManager()->hasConversationInProgress());
}

void AssistantTest::shouldStartTroubleshooterOnDemand()
{
    Assistant assistant;
    TroubleshooterClient *client = assistant.troubleshooterClient();
    QCOMPARE(client->status(), TroubleshooterClient::Status::Stopped);
    QCOMPARE(statusObject(assistant.Status()).value("troubleshooter"_L1).toObject().value("status"_L1).toString(), u"stopped"_s);

    bool ready = false;
    client->whenReady(&assistant, [&ready](const QString &error) {
        QVERIFY(error.isEmpty());
        ready = true;
    });
    QCOMPARE(client->status(), TroubleshooterClient::Status::Starting);
    QTRY_VERIFY_WITH_TIMEOUT(ready, 10000);
    QCOMPARE(client->status(), TroubleshooterClient::Status::Ready);

    QStringList names;
    const QJsonArray tools = QJsonDocument::fromJson(assistant.ListTools().toUtf8()).array();
    for (const QJsonValue &tool : tools) {
        names.append(tool["name"_L1].toString());
    }
    QVERIFY(names.contains(u"get_system_info"_s));
    QVERIFY(names.contains(u"list_audio_devices"_s));
    QCOMPARE(statusObject(assistant.Status()).value("troubleshooter"_L1).toObject().value("tools"_L1).toInt(), tools.count());
}

void AssistantTest::shouldCallToolWithoutLanguageModel()
{
    Assistant assistant;
    const QJsonObject result = callTool(assistant, u"get_system_info"_s, {});
    QVERIFY(!result.value("isError"_L1).toBool());
    QVERIFY(result.value("structuredContent"_L1).toObject().contains("kernel"_L1));
    QVERIFY(!assistant.troubleshooterClient()->hasPendingRequest());
}

void AssistantTest::shouldReturnErrorForUnknownTool()
{
    Assistant assistant;
    const QJsonObject result = callTool(assistant, u"unknown_tool"_s, u"{}"_s);
    QVERIFY(result.value("isError"_L1).toBool());
    QVERIFY(!result.value("content"_L1).toArray().first().toObject().value("text"_L1).toString().isEmpty());
}

void AssistantTest::shouldRejectInvalidArguments()
{
    Assistant assistant;
    const QJsonObject result = callTool(assistant, u"get_system_info"_s, u"[1, 2]"_s);
    QVERIFY(result.value("isError"_L1).toBool());
    // Server is not started for an invalid request
    QCOMPARE(assistant.troubleshooterClient()->status(), TroubleshooterClient::Status::Stopped);
}

void AssistantTest::shouldRefuseActionWithoutLanguageModel()
{
    Assistant assistant;
    // Actions can only be called by assistant, after a confirmation
    const QJsonObject result = callTool(assistant, u"set_audio_mute"_s, u"{\"muted\": true}"_s);
    QVERIFY(result.value("isError"_L1).toBool());
    QVERIFY(result.value("content"_L1).toArray().first().toObject().value("text"_L1).toString().contains("confirmation"_L1));
    QVERIFY(!statusObject(assistant.Status()).value("actionsAllowed"_L1).toBool());
}

#include "moc_assistanttest.cpp"
