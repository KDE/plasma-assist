/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "reportmoduletest.h"
#include "modules/reportmodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(ReportModuleTest)

ReportModuleTest::ReportModuleTest(QObject *parent)
    : QObject{parent}
{
}

void ReportModuleTest::shouldCreateMarkdown()
{
    const QJsonObject system{
        {u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, u"openSUSE Tumbleweed"_s}}}},
        {u"structuredContent"_s, QJsonObject{{u"kernel"_s, u"7.2.9"_s}}},
    };
    const QJsonObject network{
        {u"isError"_s, true},
        {u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, u"NetworkManager is not running"_s}}}},
    };
    const QString report =
        ReportModule::reportToMarkdown({{.title = u"System"_s, .result = system}, {.title = u"Network"_s, .result = network}}, u"2026-10-27T08:00:00"_s);
    QVERIFY(report.startsWith("# System Report"_L1));
    QVERIFY(report.contains("2026-10-27T08:00:00"_L1));
    QVERIFY(report.contains("## System\n\nopenSUSE Tumbleweed\n"_L1));
    QVERIFY(report.contains("```json\n"_L1));
    QVERIFY(report.contains("\"kernel\": \"7.2.9\""_L1));
    QVERIFY(report.contains("## Network\n\nNot available: NetworkManager is not running\n"_L1));
}

#include "moc_reportmoduletest.cpp"
