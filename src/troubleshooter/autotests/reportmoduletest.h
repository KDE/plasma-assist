/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class ReportModuleTest : public QObject
{
    Q_OBJECT
public:
    explicit ReportModuleTest(QObject *parent = nullptr);
    ~ReportModuleTest() override = default;

private Q_SLOTS:
    void shouldCreateMarkdown();
};
