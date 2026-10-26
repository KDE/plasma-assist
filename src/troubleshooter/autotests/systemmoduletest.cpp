/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "systemmoduletest.h"
#include "modules/systemmodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(SystemModuleTest)

SystemModuleTest::SystemModuleTest(QObject *parent)
    : QObject{parent}
{
}

void SystemModuleTest::shouldParseOsRelease()
{
    const QByteArray content =
        "NAME=\"openSUSE Tumbleweed\"\n"
        "# comment\n"
        "ID=opensuse-tumbleweed\n"
        "VERSION_ID='20261020'\n"
        "PRETTY_NAME=\"openSUSE Tumbleweed\"\n"
        "HOME_URL=\"https://www.opensuse.org/\"\n";
    const QJsonObject distribution = SystemModule::parseOsRelease(content);
    QCOMPARE(distribution.value("NAME"_L1).toString(), u"openSUSE Tumbleweed"_s);
    QCOMPARE(distribution.value("ID"_L1).toString(), u"opensuse-tumbleweed"_s);
    QCOMPARE(distribution.value("VERSION_ID"_L1).toString(), u"20261020"_s);
    QCOMPARE(distribution.value("PRETTY_NAME"_L1).toString(), u"openSUSE Tumbleweed"_s);
    // Only useful keys are kept
    QVERIFY(!distribution.contains("HOME_URL"_L1));
}

void SystemModuleTest::shouldIgnoreInvalidOsReleaseLines()
{
    QVERIFY(SystemModule::parseOsRelease({}).isEmpty());
    QVERIFY(SystemModule::parseOsRelease("NAME\n=foo\n#NAME=bar\n").isEmpty());
    QCOMPARE(SystemModule::parseOsRelease("NAME=\"\n").value("NAME"_L1).toString(), u"\""_s);
}

void SystemModuleTest::shouldDetectLowSpace_data()
{
    QTest::addColumn<qint64>("total");
    QTest::addColumn<qint64>("available");
    QTest::addColumn<bool>("lowSpace");
    QTest::newRow("empty") << qint64(0) << qint64(0) << false;
    QTest::newRow("full") << qint64(1000) << qint64(0) << true;
    QTest::newRow("4%") << qint64(1000) << qint64(40) << true;
    QTest::newRow("5%") << qint64(1000) << qint64(50) << false;
    QTest::newRow("50%") << qint64(1000) << qint64(500) << false;
}

void SystemModuleTest::shouldDetectLowSpace()
{
    QFETCH(qint64, total);
    QFETCH(qint64, available);
    QFETCH(bool, lowSpace);
    QCOMPARE(SystemModule::isLowSpace(total, available), lowSpace);
}

void SystemModuleTest::shouldMergeVolumesByDevice()
{
    const QJsonArray volumes{
        QJsonObject{{u"device"_s, u"/dev/sda2"_s}, {u"mountPoint"_s, u"/home"_s}},
        QJsonObject{{u"device"_s, u"/dev/sda2"_s}, {u"mountPoint"_s, u"/"_s}},
        QJsonObject{{u"device"_s, u"/dev/sda1"_s}, {u"mountPoint"_s, u"/boot/efi"_s}},
        QJsonObject{{u"device"_s, u"/dev/sda2"_s}, {u"mountPoint"_s, u"/opt"_s}},
    };
    const QJsonArray merged = SystemModule::mergeVolumesByDevice(volumes);
    QCOMPARE(merged.count(), 2);
    const QJsonObject root = merged.at(0).toObject();
    QCOMPARE(root.value("mountPoint"_L1).toString(), u"/"_s);
    QCOMPARE(root.value("otherMountPoints"_L1).toArray(), (QJsonArray{u"/home"_s, u"/opt"_s}));
    const QJsonObject efi = merged.at(1).toObject();
    QCOMPARE(efi.value("mountPoint"_L1).toString(), u"/boot/efi"_s);
    QVERIFY(!efi.contains("otherMountPoints"_L1));
}

void SystemModuleTest::shouldReturnSystemInfo()
{
    const QJsonObject info = SystemModule::systemInfo();
    QVERIFY(!info.value("kernel"_L1).toObject().value("version"_L1).toString().isEmpty());
    QVERIFY(!info.value("qtVersion"_L1).toString().isEmpty());
    QVERIFY(!info.value("frameworksVersion"_L1).toString().isEmpty());
    QVERIFY(info.value("graphicCards"_L1).isArray());
}

#include "moc_systemmoduletest.cpp"
