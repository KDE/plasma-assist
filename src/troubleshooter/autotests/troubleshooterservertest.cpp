/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "troubleshooterservertest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(TroubleshooterServerTest)

namespace
{
QByteArray request(int id, const QString &method, const QJsonObject &params)
{
    const QJsonObject request{
        {u"jsonrpc"_s, u"2.0"_s},
        {u"id"_s, id},
        {u"method"_s, method},
        {u"params"_s, params},
    };
    return QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
}

QByteArray initializeRequest()
{
    return request(1,
                   u"initialize"_s,
                   QJsonObject{{u"protocolVersion"_s, u"2025-11-25"_s},
                               {u"capabilities"_s, QJsonObject{}},
                               {u"clientInfo"_s, QJsonObject{{u"name"_s, u"troubleshooterservertest"_s}, {u"version"_s, u"1.0"_s}}}});
}

// Every line on stdout must be a JSON-RPC message
QJsonObject readAnswer(QProcess &process)
{
    if (!QTest::qWaitFor(
            [&process]() {
                return process.canReadLine();
            },
            10000)) {
        return {};
    }
    return QJsonDocument::fromJson(process.readLine()).object();
}

// Starts server and answers initialize
bool startServer(QProcess &process)
{
    process.start(QStringLiteral(TROUBLESHOOTER_BINARY), {});
    if (!process.waitForStarted()) {
        return false;
    }
    process.write(initializeRequest());
    return readAnswer(process).value("id"_L1).toInt() == 1;
}
}

TroubleshooterServerTest::TroubleshooterServerTest(QObject *parent)
    : QObject{parent}
{
}

void TroubleshooterServerTest::shouldAnswerInitialize()
{
    QProcess process;
    process.start(QStringLiteral(TROUBLESHOOTER_BINARY), {});
    QVERIFY(process.waitForStarted());
    process.write(initializeRequest());

    QTRY_VERIFY(process.canReadLine());
    // First line on stdout must be the answer: nothing else can be written on it
    const QJsonDocument doc = QJsonDocument::fromJson(process.readLine());
    QVERIFY(doc.isObject());
    const QJsonObject answer = doc.object();
    QCOMPARE(answer.value("id"_L1).toInt(), 1);
    QCOMPARE(answer.value("result"_L1).toObject().value("serverInfo"_L1).toObject().value("name"_L1).toString(), u"plasma-assist-troubleshooter"_s);

    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
}

void TroubleshooterServerTest::shouldListToolsWithAnnotations()
{
    QProcess process;
    QVERIFY(startServer(process));
    process.write(request(2, u"tools/list"_s, {}));
    const QJsonObject answer = readAnswer(process);
    QCOMPARE(answer.value("id"_L1).toInt(), 2);
    const QJsonArray tools = answer.value("result"_L1).toObject().value("tools"_L1).toArray();
    QVERIFY(!tools.isEmpty());

    QStringList names;
    for (const QJsonValue &value : tools) {
        const QJsonObject tool = value.toObject();
        const QString name = tool.value("name"_L1).toString();
        names.append(name);
        // Missing annotations are considered as destructive by client
        const QJsonObject annotations = tool.value("annotations"_L1).toObject();
        QVERIFY2(annotations.contains("readOnlyHint"_L1), qPrintable(name));
        QVERIFY2(!tool.value("description"_L1).toString().isEmpty(), qPrintable(name));
        QVERIFY2(!tool.value("_meta"_L1).toObject().value("org.kde/group"_L1).toString().isEmpty(), qPrintable(name));
        if (name.startsWith("get_"_L1) || name.startsWith("list_"_L1)) {
            QVERIFY2(annotations.value("readOnlyHint"_L1).toBool(), qPrintable(name));
        }
    }
    QVERIFY(names.contains(u"get_system_info"_s));
    QVERIFY(names.contains(u"get_disk_usage"_s));

    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
}

void TroubleshooterServerTest::shouldCallTool_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QString>("expectedKey");
    QTest::newRow("get_system_info") << u"get_system_info"_s << u"kernel"_s;
    QTest::newRow("get_disk_usage") << u"get_disk_usage"_s << u"volumes"_s;
}

void TroubleshooterServerTest::shouldCallTool()
{
    QFETCH(QString, tool);
    QFETCH(QString, expectedKey);

    QProcess process;
    QVERIFY(startServer(process));
    process.write(request(2, u"tools/call"_s, QJsonObject{{u"name"_s, tool}, {u"arguments"_s, QJsonObject{}}}));
    const QJsonObject answer = readAnswer(process);
    QCOMPARE(answer.value("id"_L1).toInt(), 2);
    const QJsonObject result = answer.value("result"_L1).toObject();
    QVERIFY(!result.value("isError"_L1).toBool());
    QVERIFY(result.value("structuredContent"_L1).toObject().contains(expectedKey));
    QVERIFY(!result.value("content"_L1).toArray().first().toObject().value("text"_L1).toString().isEmpty());

    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
}

void TroubleshooterServerTest::shouldExitWhenStdinIsClosed()
{
    QProcess process;
    process.start(QStringLiteral(TROUBLESHOOTER_BINARY), {});
    QVERIFY(process.waitForStarted());
    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

#include "moc_troubleshooterservertest.cpp"
