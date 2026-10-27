/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class PrivacyFilterTest : public QObject
{
    Q_OBJECT
public:
    explicit PrivacyFilterTest(QObject *parent = nullptr);
    ~PrivacyFilterTest() override = default;

private Q_SLOTS:
    void shouldMaskMacAddresses();
    void shouldMaskIpAddresses();
    void shouldMaskUserData();
    void shouldMaskSensitiveWords();
    void shouldFilterJson();
    void shouldDoNothingWhenDisabled();
};
