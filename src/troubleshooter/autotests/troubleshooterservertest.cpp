/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "troubleshooterservertest.h"
#include <QElapsedTimer>
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

// Reads messages until answer of request \a id: every line must be a JSON-RPC message (nothing else is written on stdout)
QJsonObject readAnswer(QProcess &process, int id, int timeout)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout) {
        if (!process.canReadLine() && !process.waitForReadyRead(100)) {
            continue;
        }
        while (process.canReadLine()) {
            const QByteArray line = process.readLine();
            const QJsonObject message = QJsonDocument::fromJson(line).object();
            if (message.value("jsonrpc"_L1).toString() != "2.0"_L1) {
                qWarning() << "Unexpected data on stdout:" << line;
                return {};
            }
            if (message.value("id"_L1).toInt(-1) == id) {
                return message;
            }
        }
    }
    return {};
}

// Server is a QGuiApplication: it must start without display (CI)
void startProcess(QProcess &process)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"QT_QPA_PLATFORM"_s, u"offscreen"_s);
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(TROUBLESHOOTER_BINARY), {});
}

// Starts server and answers initialize
bool startServer(QProcess &process)
{
    startProcess(process);
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
    startProcess(process);
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
    QVERIFY(names.contains(u"list_audio_devices"_s));
    QVERIFY(names.contains(u"list_audio_cards"_s));
    QVERIFY(names.contains(u"list_audio_streams"_s));
    QVERIFY(names.contains(u"list_outputs"_s));
    const QStringList m3Tools{u"get_network_status"_s,
                              u"list_connections"_s,
                              u"check_connectivity"_s,
                              u"get_power_status"_s,
                              u"list_sleep_inhibitors"_s,
                              u"list_recent_crashes"_s,
                              u"get_crash_backtrace"_s,
                              u"read_journal"_s,
                              u"get_system_report"_s};
    for (const QString &tool : m3Tools) {
        QVERIFY2(names.contains(tool), qPrintable(tool));
    }

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
    const QJsonArray content = result.value("content"_L1).toArray();
    QVERIFY(!content.first().toObject().value("text"_L1).toString().isEmpty());
    // Structured content is also sent as text
    QCOMPARE(QJsonDocument::fromJson(content.last().toObject().value("text"_L1).toString().toUtf8()).object(), result.value("structuredContent"_L1).toObject());

    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
}

void TroubleshooterServerTest::shouldExitWhenStdinIsClosed()
{
    QProcess process;
    startProcess(process);
    QVERIFY(process.waitForStarted());
    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void TroubleshooterServerTest::shouldAnswerEveryTool()
{
    QProcess process;
    QVERIFY(startServer(process));
    process.write(request(2, u"tools/list"_s, {}));
    const QJsonArray tools = readAnswer(process).value("result"_L1).toObject().value("tools"_L1).toArray();
    QVERIFY(!tools.isEmpty());

    int id = 10;
    for (const QJsonValue &value : tools) {
        const QJsonObject tool = value.toObject();
        const QString name = tool.value("name"_L1).toString();
        // Tests don't contact Internet
        if (tool.value("annotations"_L1).toObject().value("openWorldHint"_L1).toBool()) {
            continue;
        }
        QJsonObject arguments;
        if (name == "get_crash_backtrace"_L1) {
            arguments.insert(u"pid"_s, 1);
        }
        process.write(request(++id, u"tools/call"_s, QJsonObject{{u"name"_s, name}, {u"arguments"_s, arguments}}));
        const QJsonObject answer = readAnswer(process, id, 60000);
        // System can miss services (CI): tools answer with isError, never with a JSON-RPC error
        QVERIFY2(answer.contains("result"_L1), qPrintable(name));
        QVERIFY2(!answer.value("result"_L1).toObject().value("content"_L1).toArray().isEmpty(), qPrintable(name));
    }

    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
}

void TroubleshooterServerTest::shouldProvideReportResource()
{
    QProcess process;
    QVERIFY(startServer(process));
    process.write(request(2, u"resources/list"_s, {}));
    const QJsonArray resources = readAnswer(process, 2, 10000).value("result"_L1).toObject().value("resources"_L1).toArray();
    QCOMPARE(resources.count(), 1);
    QCOMPARE(resources.first().toObject().value("uri"_L1).toString(), u"system://report"_s);

    process.write(request(3, u"resources/read"_s, QJsonObject{{u"uri"_s, u"system://report"_s}}));
    const QJsonArray contents = readAnswer(process, 3, 10000).value("result"_L1).toObject().value("contents"_L1).toArray();
    QCOMPARE(contents.count(), 1);
    // Report is generated by tool
    QVERIFY(contents.first().toObject().value("text"_L1).toString().contains("get_system_report"_L1));

    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
}

#include "moc_troubleshooterservertest.cpp"
