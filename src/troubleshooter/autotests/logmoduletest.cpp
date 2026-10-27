/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "logmoduletest.h"
#include "modules/logmodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(LogModuleTest)

LogModuleTest::LogModuleTest(QObject *parent)
    : QObject{parent}
{
}

void LogModuleTest::shouldBuildArguments()
{
    LogModule::JournalArguments result = LogModule::journalArguments({});
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.limit, 50);
    QVERIFY(result.arguments.contains(u"--user"_s));

    result = LogModule::journalArguments({
        {u"unit"_s, u"NetworkManager.service"_s},
        {u"executable"_s, u"plasmashell"_s},
        {u"since"_s, u"2h"_s},
        {u"priority"_s, u"err"_s},
        {u"limit"_s, 1000},
        {u"system"_s, true},
    });
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.limit, 200);
    QVERIFY(!result.arguments.contains(u"--user"_s));
    QVERIFY(result.arguments.contains(u"NetworkManager.service"_s));
    QVERIFY(result.arguments.contains(u"_COMM=plasmashell"_s));
    QVERIFY(result.arguments.contains(u"--since=-2h"_s));
    QVERIFY(result.arguments.contains(u"err"_s));

    // Absolute dates are kept
    result = LogModule::journalArguments({{u"since"_s, u"2026-10-27 08:00"_s}});
    QVERIFY(result.arguments.contains(u"--since=2026-10-27 08:00"_s));
}

void LogModuleTest::shouldRejectInvalidArguments()
{
    // Values can't be read as options or patterns
    QVERIFY(!LogModule::journalArguments({{u"unit"_s, u"--help"_s}}).error.isEmpty());
    QVERIFY(!LogModule::journalArguments({{u"unit"_s, u"a b"_s}}).error.isEmpty());
    QVERIFY(!LogModule::journalArguments({{u"executable"_s, u"-f"_s}}).error.isEmpty());
    QVERIFY(!LogModule::journalArguments({{u"executable"_s, u"/usr/bin/plasmashell"_s}}).error.isEmpty());
    QVERIFY(!LogModule::journalArguments({{u"since"_s, u"--follow"_s}}).error.isEmpty());
    QVERIFY(!LogModule::journalArguments({{u"priority"_s, u"verbose"_s}}).error.isEmpty());
}

void LogModuleTest::shouldParseEntries()
{
    const QByteArray output =
        R"({"MESSAGE":"Wi-Fi connected","PRIORITY":"6","SYSLOG_IDENTIFIER":"NetworkManager","_PID":"1725","__REALTIME_TIMESTAMP":"1793082053801955"}
{"MESSAGE":[72,105],"PRIORITY":"3","_COMM":"kwin_wayland","_PID":"42","__REALTIME_TIMESTAMP":"1793082053801955"}
not json
)";
    const QJsonObject result = LogModule::entriesToJson(output);
    QCOMPARE(result.value("count"_L1).toInt(), 2);
    const QJsonArray entries = result.value("entries"_L1).toArray();
    QCOMPARE(entries.at(0).toObject().value("identifier"_L1).toString(), u"NetworkManager"_s);
    QCOMPARE(entries.at(0).toObject().value("priority"_L1).toString(), u"info"_s);
    // Binary message and fallback to process name
    QCOMPARE(entries.at(1).toObject().value("message"_L1).toString(), u"Hi"_s);
    QCOMPARE(entries.at(1).toObject().value("identifier"_L1).toString(), u"kwin_wayland"_s);
    QCOMPARE(entries.at(1).toObject().value("priority"_L1).toString(), u"err"_s);
}

#include "moc_logmoduletest.cpp"
