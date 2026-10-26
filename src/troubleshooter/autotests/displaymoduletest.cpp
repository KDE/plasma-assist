/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "displaymoduletest.h"
#include "modules/displaymodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(DisplayModuleTest)

namespace
{
DisplayOutput laptopPanel()
{
    DisplayOutput output;
    output.id = 1;
    output.name = u"eDP-1"_s;
    output.type = u"Panel"_s;
    output.modes = {DisplayMode{.id = u"1"_s, .size = QSize(1920, 1200), .refreshRate = 60}};
    output.currentModeId = u"1"_s;
    output.preferredModeId = u"1"_s;
    output.rotation = u"none"_s;
    output.priority = 1;
    output.connected = true;
    output.enabled = true;
    return output;
}

DisplayOutput externalHdmi()
{
    DisplayOutput output;
    output.id = 2;
    output.name = u"HDMI-A-1"_s;
    output.type = u"HDMI"_s;
    output.modes = {DisplayMode{.id = u"10"_s, .size = QSize(3840, 2160), .refreshRate = 60},
                    DisplayMode{.id = u"11"_s, .size = QSize(3840, 2160), .refreshRate = 30},
                    DisplayMode{.id = u"12"_s, .size = QSize(1920, 1080), .refreshRate = 60}};
    output.currentModeId = u"10"_s;
    output.preferredModeId = u"10"_s;
    output.position = QPoint(1920, 0);
    output.rotation = u"none"_s;
    output.priority = 2;
    output.connected = true;
    output.enabled = true;
    return output;
}
}

DisplayModuleTest::DisplayModuleTest(QObject *parent)
    : QObject{parent}
{
}

void DisplayModuleTest::shouldHaveNoHintWhenEverythingIsFine()
{
    QVERIFY(DisplayModule::hints({laptopPanel(), externalHdmi()}).isEmpty());
}

void DisplayModuleTest::shouldDetectConnectedButDisabledOutput()
{
    // HDMI connected but disabled
    DisplayOutput hdmi = externalHdmi();
    hdmi.enabled = false;
    QCOMPARE(DisplayModule::hints({laptopPanel(), hdmi}), QStringList{u"Output HDMI-A-1 is connected but disabled"_s});
}

void DisplayModuleTest::shouldDetectNotNativeMode()
{
    DisplayOutput hdmi = externalHdmi();
    hdmi.currentModeId = u"12"_s;
    QCOMPARE(DisplayModule::hints({hdmi}), QStringList{u"Output HDMI-A-1 uses mode 1920x1080@60Hz instead of its native mode 3840x2160@60Hz"_s});

    // Lower refresh rate with native size is not a problem
    hdmi.currentModeId = u"11"_s;
    QVERIFY(DisplayModule::hints({hdmi}).isEmpty());
}

void DisplayModuleTest::shouldDetectRotation()
{
    DisplayOutput panel = laptopPanel();
    panel.rotation = u"left"_s;
    QCOMPARE(DisplayModule::hints({panel}), QStringList{u"Output eDP-1 is rotated (left)"_s});
}

void DisplayModuleTest::shouldDetectNoEnabledOutput()
{
    DisplayOutput panel = laptopPanel();
    panel.enabled = false;
    QCOMPARE(DisplayModule::hints({panel}), (QStringList{u"Output eDP-1 is connected but disabled"_s, u"No connected output is enabled"_s}));
}

void DisplayModuleTest::shouldListDisconnectedOutputsByName()
{
    DisplayOutput hdmi = externalHdmi();
    hdmi.connected = false;
    const QJsonObject result = DisplayModule::outputsToJson({laptopPanel(), hdmi});
    QCOMPARE(result.value("outputs"_L1).toArray().count(), 1);
    QCOMPARE(result.value("disconnectedOutputs"_L1).toArray(), QJsonArray{u"HDMI-A-1"_s});

    const QJsonObject panel = result.value("outputs"_L1).toArray().first().toObject();
    QCOMPARE(panel.value("name"_L1).toString(), u"eDP-1"_s);
    QCOMPARE(panel.value("primary"_L1).toBool(), true);
    QCOMPARE(panel.value("currentMode"_L1).toObject().value("size"_L1).toString(), u"1920x1200"_s);
}

void DisplayModuleTest::shouldGroupModesBySize()
{
    const QJsonObject output = DisplayModule::outputsToJson({externalHdmi()}).value("outputs"_L1).toArray().first().toObject();
    const QJsonArray modes = output.value("availableModes"_L1).toArray();
    QCOMPARE(modes.count(), 2);
    QCOMPARE(modes.at(0).toObject().value("size"_L1).toString(), u"3840x2160"_s);
    QCOMPARE(modes.at(0).toObject().value("refreshRates"_L1).toArray(), (QJsonArray{60, 30}));
    QCOMPARE(modes.at(1).toObject().value("size"_L1).toString(), u"1920x1080"_s);
}

void DisplayModuleTest::shouldLimitModes()
{
    DisplayOutput output = laptopPanel();
    output.modes.clear();
    for (int i = 0; i < 30; ++i) {
        output.modes.append(DisplayMode{.id = QString::number(i), .size = QSize(640 + i * 10, 480), .refreshRate = 60});
    }
    const QJsonObject result = DisplayModule::outputsToJson({output}).value("outputs"_L1).toArray().first().toObject();
    QCOMPARE(result.value("availableModes"_L1).toArray().count(), 10);
}

#include "moc_displaymoduletest.cpp"
