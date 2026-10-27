/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "crashmoduletest.h"
#include "modules/crashmodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(CrashModuleTest)

CrashModuleTest::CrashModuleTest(QObject *parent)
    : QObject{parent}
{
}

void CrashModuleTest::shouldParseCrashes()
{
    // Output of coredumpctl --json=short -r list
    const QByteArray output =
        R"([{"time":1793033981530385,"pid":387700,"uid":1000,"gid":1000,"sig":11,"corefile":"present","exe":"/usr/bin/plasmashell","size":283003},
        {"time":1793006162530520,"pid":73104,"uid":1000,"gid":1000,"sig":6,"corefile":"missing","exe":"/usr/bin/kmail","size":null},
        {"time":1793006100000000,"pid":73000,"uid":1000,"gid":1000,"sig":6,"corefile":"missing","exe":"/usr/bin/kmail","size":null}])";
    const QJsonObject result = CrashModule::crashesToJson(output, 2);
    QCOMPARE(result.value("total"_L1).toInt(), 3);
    QVERIFY(!result.value("totalIsCapped"_L1).toBool());
    const QJsonArray crashes = result.value("crashes"_L1).toArray();
    QCOMPARE(crashes.count(), 2);
    const QJsonObject first = crashes.first().toObject();
    QCOMPARE(first.value("application"_L1).toString(), u"plasmashell"_s);
    QCOMPARE(first.value("signal"_L1).toString(), u"SIGSEGV"_s);
    QCOMPARE(first.value("pid"_L1).toInt(), 387700);
    QCOMPARE(first.value("coreFile"_L1).toString(), u"present"_s);
    const QJsonObject mostCrashed = result.value("crashesByApplication"_L1).toArray().first().toObject();
    QCOMPARE(mostCrashed.value("application"_L1).toString(), u"kmail"_s);
    QCOMPARE(mostCrashed.value("count"_L1).toInt(), 2);
    QCOMPARE(CrashModule::signalName(42), u"42"_s);
}

void CrashModuleTest::shouldParseBacktrace()
{
    // Output of coredumpctl info
    const QByteArray output = R"(           PID: 387700 (plasmashell)
        Signal: 11 (SEGV)
     Timestamp: Mon 2026-10-26 17:59:37 CET (13h ago)
  Command Line: /usr/bin/plasmashell --token=secret
    Executable: /usr/bin/plasmashell
       Boot ID: c80bdee3aaa8433fa2e13cd8dcca5567
       Storage: /var/lib/systemd/coredump/core.plasmashell.zst (present)
       Message: Process 387700 (plasmashell) of user 1000 dumped core.

                Stack trace of thread 387700:
                #0  0x00007f067c216d00 QObject::parent (libQt6Core.so.6 + 0x2d00)
                #1  0x00007f067c216d01 Applet::init (libplasma.so.6 + 0x100)
                #2  0x00007f067c216d02 main (plasmashell + 0x10)

                Stack trace of thread 387701:
                #0  0x00007f067c216d03 poll (libc.so.6 + 0x5)
                ELF object binary architecture: AMD x86-64
)";
    const QJsonObject result = CrashModule::backtraceToJson(output, 2);
    QCOMPARE(result.value("executable"_L1).toString(), u"/usr/bin/plasmashell"_s);
    QCOMPARE(result.value("signal"_L1).toString(), u"11 (SEGV)"_s);
    QCOMPARE(result.value("thread"_L1).toString(), u"387700"_s);
    QCOMPARE(result.value("otherThreads"_L1).toInt(), 1);
    QCOMPARE(result.value("frames"_L1).toArray(),
             (QJsonArray{u"#0 QObject::parent (libQt6Core.so.6 + 0x2d00)"_s, u"#1 Applet::init (libplasma.so.6 + 0x100)"_s}));
    QVERIFY(result.value("hasBacktrace"_L1).toBool());
    // Command line can contain secrets, boot id is useless
    QVERIFY(!result.contains("commandLine"_L1));
    QVERIFY(!result.toVariantMap().values().contains(u"c80bdee3aaa8433fa2e13cd8dcca5567"_s));
}

void CrashModuleTest::shouldIgnoreUnknownOutput()
{
    QVERIFY(CrashModule::backtraceToJson("garbage", 10).isEmpty());
    QCOMPARE(CrashModule::crashesToJson("not json", 10).value("total"_L1).toInt(), 0);
}

#include "moc_crashmoduletest.cpp"
