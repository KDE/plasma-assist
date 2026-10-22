/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "troubleshooterservertest.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(TroubleshooterServerTest)

namespace
{
QByteArray initializeRequest()
{
    const QJsonObject request{
        {u"jsonrpc"_s, u"2.0"_s},
        {u"id"_s, 1},
        {u"method"_s, u"initialize"_s},
        {u"params"_s,
         QJsonObject{{u"protocolVersion"_s, u"2025-11-25"_s},
                     {u"capabilities"_s, QJsonObject{}},
                     {u"clientInfo"_s, QJsonObject{{u"name"_s, u"troubleshooterservertest"_s}, {u"version"_s, u"1.0"_s}}}}},
    };
    return QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
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
