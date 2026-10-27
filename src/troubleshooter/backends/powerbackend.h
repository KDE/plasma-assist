/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <functional>

class QObject;

struct BatteryInfo {
    QString name;
    QString type; // Primary, Mouse, Keyboard…
    QString chargeState; // Charging, Discharging, FullyCharged, NoCharge
    int chargePercent = 0;
    int capacityPercent = 100; // health: capacity compared to design capacity
    qlonglong timeToEmptySec = 0;
    bool present = true;
    bool powerSupply = true; // battery of computer
};

struct PowerStatus {
    bool onBattery = false;
    QList<BatteryInfo> batteries;
    QString powerProfile; // power-saver, balanced, performance (empty if not supported)
    QStringList powerProfileChoices;
    // Settings changed by user in Power Management settings, by profile and group ("AC/SuspendAndShutdown" → key → value)
    QMap<QString, QMap<QString, QString>> settings;
};

struct SleepInhibitor {
    QString source; // powerdevil or logind
    QString what; // sleep, idle…
    QString who; // application
    QString why;
    QString mode; // block or delay
    uint pid = 0;
};

// Batteries, power profile, sleep settings and applications which block sleep
class PowerBackend
{
public:
    using StatusCallback = std::function<void(const PowerStatus &status, const QString &error)>;
    using InhibitorsCallback = std::function<void(const QList<SleepInhibitor> &inhibitors, const QString &error)>;

    PowerBackend();
    virtual ~PowerBackend();

    virtual void fetchStatus(QObject *context, const StatusCallback &callback) = 0;
    virtual void fetchInhibitors(QObject *context, const InhibitorsCallback &callback) = 0;

private:
    Q_DISABLE_COPY_MOVE(PowerBackend)
};
