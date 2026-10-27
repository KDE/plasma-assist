/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "powerbackend.h"

// PowerBackend implemented with Solid, UPower, PowerDevil and logind (D-Bus)
class SystemPowerBackend : public PowerBackend
{
public:
    SystemPowerBackend();
    ~SystemPowerBackend() override;

    void fetchStatus(QObject *context, const StatusCallback &callback) override;
    void fetchInhibitors(QObject *context, const InhibitorsCallback &callback) override;
};
