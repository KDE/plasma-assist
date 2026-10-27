/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "powermoduletest.h"
#include "modules/powermodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(PowerModuleTest)

PowerModuleTest::PowerModuleTest(QObject *parent)
    : QObject{parent}
{
}

void PowerModuleTest::shouldReportBatteryProblems()
{
    PowerStatus status;
    status.onBattery = true;
    status.powerProfile = u"performance"_s;
    status.batteries = {{.name = u"BAT0"_s,
                         .type = u"PrimaryBattery"_s,
                         .chargeState = u"Discharging"_s,
                         .chargePercent = 8,
                         .capacityPercent = 45,
                         .timeToEmptySec = 600,
                         .present = true,
                         .powerSupply = true},
                        // Battery of a mouse is ignored
                        {.name = u"Mouse"_s,
                         .type = u"MouseBattery"_s,
                         .chargeState = u"Discharging"_s,
                         .chargePercent = 5,
                         .capacityPercent = 100,
                         .timeToEmptySec = 0,
                         .present = true,
                         .powerSupply = false}};
    const QStringList hints = PowerModule::hints(status);
    QCOMPARE(hints.count(), 3);
    QVERIFY(hints.at(0).contains("is low (8%)"_L1));
    QVERIFY(hints.at(1).contains("worn"_L1));
    QVERIFY(hints.at(2).contains("Performance"_L1));

    const QJsonObject result = PowerModule::statusToJson(status);
    QCOMPARE(result.value("batteries"_L1).toArray().first().toObject().value("timeToEmptyMinutes"_L1).toInt(), 10);
}

void PowerModuleTest::shouldReportDisabledSleep()
{
    PowerStatus status;
    status.settings.insert(u"AC/SuspendAndShutdown"_s, {{u"AutoSuspendAction"_s, u"0"_s}});
    QCOMPARE(PowerModule::hints(status), QStringList{u"Automatic sleep is disabled in profile AC"_s});
    QCOMPARE(
        PowerModule::statusToJson(status).value("changedSettings"_L1).toObject().value("AC/SuspendAndShutdown"_L1).toObject().value("AutoSuspendAction"_L1),
        u"0"_s);
}

void PowerModuleTest::shouldKeepBlockingInhibitors()
{
    const QList<SleepInhibitor> inhibitors{
        {.source = u"powerdevil"_s, .what = u"sleep"_s, .who = u"VLC"_s, .why = u"Playing video"_s, .mode = u"block"_s, .pid = 0},
        // Delays sleep a few seconds only
        {.source = u"logind"_s, .what = u"sleep"_s, .who = u"NetworkManager"_s, .why = u"Turn off networks"_s, .mode = u"delay"_s, .pid = 1725},
        {.source = u"logind"_s,
         .what = u"handle-power-key:handle-lid-switch"_s,
         .who = u"PowerDevil"_s,
         .why = u"KDE handles keys"_s,
         .mode = u"block"_s,
         .pid = 1},
        {.source = u"logind"_s, .what = u"shutdown:sleep"_s, .who = u"Steam"_s, .why = u"Downloading"_s, .mode = u"block"_s, .pid = 42},
    };
    const QList<SleepInhibitor> blocking = PowerModule::blockingInhibitors(inhibitors);
    QCOMPARE(blocking.count(), 2);
    QCOMPARE(blocking.at(0).who, u"VLC"_s);
    QCOMPARE(blocking.at(1).who, u"Steam"_s);
    const QJsonArray array = PowerModule::inhibitorsToJson(blocking).value("inhibitors"_L1).toArray();
    QVERIFY(!array.at(0).toObject().contains("pid"_L1));
    QCOMPARE(array.at(1).toObject().value("pid"_L1).toInt(), 42);
}

#include "moc_powermoduletest.cpp"
