/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class NetworkModuleTest : public QObject
{
    Q_OBJECT
public:
    explicit NetworkModuleTest(QObject *parent = nullptr);
    ~NetworkModuleTest() override = default;

private Q_SLOTS:
    void shouldReportWifiDisabled();
    void shouldReportCaptivePortal();
    void shouldReportNoConnection();
    void shouldNotReportProblemWhenConnected();
    void shouldConvertStatus();
    void shouldValidateHostNames();
};
