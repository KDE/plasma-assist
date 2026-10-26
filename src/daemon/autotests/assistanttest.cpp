/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "assistanttest.h"
#include "assistant.h"
#include "troubleshooterclient.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
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
}

void AssistantTest::shouldHaveDefaultStatus()
{
    const Assistant assistant;
    const QJsonObject status = statusObject(assistant.Status());
    QVERIFY(!status.value("version"_L1).toString().isEmpty());
    QVERIFY(!status.value("ready"_L1).toBool());
    QCOMPARE(status.value("conversations"_L1).toInt(), 0);
}

void AssistantTest::shouldCreateConversation()
{
    Assistant assistant;
    QSignalSpy statusChanged(&assistant, &Assistant::StatusChanged);
    const QString first = assistant.NewConversation();
    const QString second = assistant.NewConversation();
    QVERIFY(!first.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(statusChanged.count(), 2);
    QCOMPARE(statusObject(statusChanged.last().at(0).toString()).value("conversations"_L1).toInt(), 2);
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

#include "moc_assistanttest.cpp"
