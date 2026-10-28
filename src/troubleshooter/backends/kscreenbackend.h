/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "displaybackend.h"

// DisplayBackend implemented with libkscreen
class KScreenBackend : public DisplayBackend
{
public:
    KScreenBackend();
    ~KScreenBackend() override;

    void fetchOutputs(QObject *context, OutputsCallback callback) override;
    void applyOutputSettings(const QString &name, const DisplayOutputSettings &settings, QObject *context, ErrorCallback callback) override;
};
