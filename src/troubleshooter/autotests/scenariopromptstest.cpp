/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scenariopromptstest.h"
#include "modules/bugreportmodule.h"
#include "prompts/scenarioprompts.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(ScenarioPromptsTest)

ScenarioPromptsTest::ScenarioPromptsTest(QObject *parent)
    : QObject{parent}
{
}

void ScenarioPromptsTest::shouldDefineScenarios()
{
    const QList<ScenarioPrompts::Scenario> scenarios = ScenarioPrompts::scenarios();
    QVERIFY(scenarios.count() >= 5);
    for (const ScenarioPrompts::Scenario &scenario : scenarios) {
        QVERIFY2(!scenario.title.isEmpty(), qPrintable(scenario.name));
        QVERIFY2(!scenario.steps.isEmpty(), qPrintable(scenario.name));
        QVERIFY2(!scenario.diagnosticTools.isEmpty(), qPrintable(scenario.name));
        // First step is a diagnostic
        QVERIFY2(scenario.steps.first().startsWith(u"list_"_s) || scenario.steps.first().startsWith(u"get_"_s), qPrintable(scenario.name));
    }
}

void ScenarioPromptsTest::shouldCreatePromptText()
{
    const ScenarioPrompts::Scenario scenario = ScenarioPrompts::scenarios().first();
    const QString text = ScenarioPrompts::promptText(scenario, u"  since yesterday  "_s, u"French (français)"_s);
    QVERIFY(text.startsWith(scenario.request));
    QVERIFY(text.contains("Details: since yesterday\n"_L1));
    QVERIFY(text.contains("1. %1"_L1.arg(scenario.steps.first())));
    QVERIFY(text.contains("Diagnose before acting"_L1));
    QVERIFY(text.contains("never write a tool call"_L1));
    QVERIFY(text.endsWith(u"Answer in French (français)."_s));
    // No details
    QVERIFY(!ScenarioPrompts::promptText(scenario, {}, u"English"_s).contains("Details"_L1));
}

void ScenarioPromptsTest::shouldCreateBugReport()
{
    BugReportModule::ReportData data;
    QVERIFY(BugReportModule::reportToMarkdown(data).contains("(Describe what you were doing"_L1));

    data.description = u"KMail crashed when opening a folder"_s;
    data.crash = QJsonObject{
        {u"executable"_s, u"/usr/bin/kmail"_s},
        {u"signal"_s, u"6 (ABRT)"_s},
        {u"dateTime"_s, u"Mon 2026-10-26 10:15:54 CET"_s},
        {u"frames"_s, QJsonArray{u"#0 raise (libc.so.6 + 0x433c6)"_s}},
    };
    data.system = QJsonObject{
        {u"distribution"_s, QJsonObject{{u"PRETTY_NAME"_s, u"openSUSE Tumbleweed"_s}}},
        {u"graphicCards"_s, QJsonArray{QJsonObject{{u"driver"_s, u"amdgpu"_s}}}},
    };
    data.journal =
        QJsonObject{{u"entries"_s, QJsonArray{QJsonObject{{u"dateTime"_s, u"10:15:50"_s}, {u"identifier"_s, u"kmail"_s}, {u"message"_s, u"Fatal error"_s}}}}};
    const QString report = BugReportModule::reportToMarkdown(data);
    QVERIFY(report.contains("KMail crashed when opening a folder"_L1));
    QVERIFY(report.contains("- Application: /usr/bin/kmail"_L1));
    QVERIFY(report.contains("```\n#0 raise (libc.so.6 + 0x433c6)\n```"_L1));
    QVERIFY(report.contains("- Distribution: openSUSE Tumbleweed"_L1));
    QVERIFY(report.contains("- Graphic driver: amdgpu"_L1));
    QVERIFY(report.contains("10:15:50 kmail: Fatal error"_L1));
    QVERIFY(report.contains("https://bugs.kde.org"_L1));
}

#include "moc_scenariopromptstest.cpp"
