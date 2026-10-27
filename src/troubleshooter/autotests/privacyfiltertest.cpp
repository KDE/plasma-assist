/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "privacyfiltertest.h"
#include "core/privacyfilter.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(PrivacyFilterTest)

namespace
{
std::unique_ptr<PrivacyFilter> createFilter()
{
    auto filter = std::make_unique<PrivacyFilter>();
    filter->setUserName(u"jdoe"_s);
    filter->setHomePath(u"/home/jdoe"_s);
    filter->setHostName(u"workstation"_s);
    return filter;
}
}

PrivacyFilterTest::PrivacyFilterTest(QObject *parent)
    : QObject{parent}
{
}

void PrivacyFilterTest::shouldMaskMacAddresses()
{
    const auto filter = createFilter();
    QCOMPARE(filter->filter(u"Device AA:BB:CC:DD:EE:FF connected"_s), u"Device <mac-1> connected"_s);
    // Same device in name of Bluetooth sink gets same pseudonym
    QCOMPARE(filter->filter(u"bluez_output.aa_bb_cc_dd_ee_ff.1"_s), u"bluez_output.<mac-1>.1"_s);
    QCOMPARE(filter->filter(u"11-22-33-44-55-66"_s), u"<mac-2>"_s);
    QCOMPARE(filter->originalValue(u"<mac-1>"_s), u"AA:BB:CC:DD:EE:FF"_s);
}

void PrivacyFilterTest::shouldMaskIpAddresses()
{
    const auto filter = createFilter();
    QCOMPARE(filter->filter(u"gateway 192.168.1.1, address 192.168.1.42/24"_s), u"gateway <ip-1>, address <ip-2>/24"_s);
    QCOMPARE(filter->filter(u"dns 2a01:e0a:1::1"_s), u"dns <ip-3>"_s);
    // Loopback addresses, versions and times are kept
    QCOMPARE(filter->filter(u"127.0.0.1 ::1 Qt 6.11.2 at 12:30:45"_s), u"127.0.0.1 ::1 Qt 6.11.2 at 12:30:45"_s);
}

void PrivacyFilterTest::shouldMaskUserData()
{
    const auto filter = createFilter();
    QCOMPARE(filter->filter(u"/home/jdoe/.config/kdeglobals"_s), u"~/.config/kdeglobals"_s);
    QCOMPARE(filter->filter(u"user jdoe on workstation"_s), u"user <user> on <hostname>"_s);
    // Only whole words
    QCOMPARE(filter->filter(u"jdoes workstations"_s), u"jdoes workstations"_s);
}

void PrivacyFilterTest::shouldMaskSensitiveWords()
{
    const auto filter = createFilter();
    filter->addWordsProvider([]() {
        return QStringList{u"Home Wifi"_s, u"Home Wifi 5G"_s, u"ab"_s};
    });
    QCOMPARE(filter->filter(u"Connected to Home Wifi 5G, saved: Home Wifi"_s), u"Connected to <name-1>, saved: <name-2>"_s);
    // Too short words are not masked
    QCOMPARE(filter->filter(u"ab"_s), u"ab"_s);
}

void PrivacyFilterTest::shouldFilterJson()
{
    const auto filter = createFilter();
    const QJsonObject result = filter
                                   ->filter(QJsonObject{
                                       {u"path"_s, u"/home/jdoe/file"_s},
                                       {u"addresses"_s, QJsonArray{u"10.0.0.2"_s, 42}},
                                       {u"/home/jdoe"_s, true},
                                   })
                                   .toObject();
    QCOMPARE(result.value("path"_L1).toString(), u"~/file"_s);
    QCOMPARE(result.value("addresses"_L1).toArray(), (QJsonArray{u"<ip-1>"_s, 42}));
    QVERIFY(result.value("~"_L1).toBool());
}

void PrivacyFilterTest::shouldDoNothingWhenDisabled()
{
    const auto filter = createFilter();
    filter->setEnabled(false);
    QCOMPARE(filter->filter(u"/home/jdoe 192.168.1.1"_s), u"/home/jdoe 192.168.1.1"_s);
}

#include "moc_privacyfiltertest.cpp"
