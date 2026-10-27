/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "networkmoduletest.h"
#include "modules/networkmodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(NetworkModuleTest)

namespace
{
NetworkInterface wifi()
{
    NetworkInterface interface;
    interface.name = u"wlan0"_s;
    interface.type = u"Wifi"_s;
    interface.state = u"Activated"_s;
    interface.connection = u"Home"_s;
    interface.ipv4Addresses = {u"192.168.1.42/24"_s};
    interface.gateway = u"192.168.1.1"_s;
    interface.nameServers = {u"192.168.1.1"_s};
    interface.ssid = u"Home"_s;
    interface.signalStrength = 70;
    return interface;
}

NetworkStatus connected()
{
    NetworkStatus status;
    status.state = u"Connected"_s;
    status.connectivity = u"Full"_s;
    status.primaryConnection = u"Home"_s;
    status.interfaces = {wifi()};
    return status;
}
}

NetworkModuleTest::NetworkModuleTest(QObject *parent)
    : QObject{parent}
{
}

void NetworkModuleTest::shouldReportWifiDisabled()
{
    NetworkStatus status = connected();
    status.interfaces.first().state = u"Unavailable"_s;
    status.wirelessEnabled = false;
    QStringList hints = NetworkModule::hints(status);
    QVERIFY(hints.contains(u"Wi-Fi is disabled"_s));
    QVERIFY(hints.contains(u"No active network connection"_s));

    status.wirelessHardwareEnabled = false;
    hints = NetworkModule::hints(status);
    QVERIFY(hints.first().contains("hardware switch"_L1));
}

void NetworkModuleTest::shouldReportCaptivePortal()
{
    NetworkStatus status = connected();
    status.connectivity = u"Portal"_s;
    QVERIFY(NetworkModule::hints(status).first().contains("captive portal"_L1));
}

void NetworkModuleTest::shouldReportNoConnection()
{
    NetworkStatus status = connected();
    status.interfaces.first().gateway.clear();
    status.interfaces.first().signalStrength = 10;
    const QStringList hints = NetworkModule::hints(status);
    QVERIFY(hints.contains(u"No default gateway: only local network can be reached"_s));
    QVERIFY(hints.contains(u"Wi-Fi signal of wlan0 is weak (10%)"_s));

    status.networkingEnabled = false;
    QCOMPARE(NetworkModule::hints(status), QStringList{u"Networking is disabled in NetworkManager"_s});
}

void NetworkModuleTest::shouldNotReportProblemWhenConnected()
{
    NetworkStatus status = connected();
    // Bridge of virtual machines has no gateway nor DNS
    NetworkInterface bridge;
    bridge.name = u"virbr0"_s;
    bridge.type = u"Bridge"_s;
    bridge.state = u"Activated"_s;
    bridge.ipv4Addresses = {u"192.168.122.1/24"_s};
    status.interfaces.append(bridge);
    QCOMPARE(NetworkModule::hints(status), QStringList());
}

void NetworkModuleTest::shouldConvertStatus()
{
    const QJsonObject result = NetworkModule::statusToJson(connected());
    QCOMPARE(result.value("connectivity"_L1).toString(), u"Full"_s);
    const QJsonObject interface = result.value("interfaces"_L1).toArray().first().toObject();
    QCOMPARE(interface.value("ssid"_L1).toString(), u"Home"_s);
    QCOMPARE(interface.value("signalStrength"_L1).toInt(), 70);
    QCOMPARE(interface.value("nameServers"_L1).toArray().count(), 1);

    const QJsonObject connections =
        NetworkModule::connectionsToJson({{.name = u"VPN"_s, .type = u"vpn"_s, .active = false, .autoConnect = false, .vpn = true}});
    QVERIFY(connections.value("connections"_L1).toArray().first().toObject().value("vpn"_L1).toBool());
}

void NetworkModuleTest::shouldValidateHostNames()
{
    QVERIFY(NetworkModule::isValidHostName(u"kde.org"_s));
    QVERIFY(NetworkModule::isValidHostName(u"invent.kde.org"_s));
    QVERIFY(!NetworkModule::isValidHostName(u"-kde.org"_s));
    QVERIFY(!NetworkModule::isValidHostName(u"kde.org/path"_s));
    QVERIFY(!NetworkModule::isValidHostName(u"http://kde.org"_s));
    QVERIFY(!NetworkModule::isValidHostName(QString()));
}

#include "moc_networkmoduletest.cpp"
