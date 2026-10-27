/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scenariomodeltest.h"
#include "scenariomodel.h"
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(ScenarioModelTest)

ScenarioModelTest::ScenarioModelTest(QObject *parent)
    : QObject{parent}
{
}

void ScenarioModelTest::shouldHaveScenarios()
{
    const ScenarioModel model;
    QVERIFY(model.rowCount() > 0);
    QStringList identifiers;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row);
        identifiers.append(index.data(ScenarioModel::IdentifierRole).toString());
        QVERIFY(!index.data(ScenarioModel::TitleRole).toString().isEmpty());
        QVERIFY(!index.data(ScenarioModel::IconNameRole).toString().isEmpty());
    }
    QVERIFY(identifiers.contains(u"no_sound"_s));
    QCOMPARE(model.roleNames().value(ScenarioModel::DescriptionRole), "description"_ba);
}

void ScenarioModelTest::shouldUseScenariosOfServer()
{
    ScenarioModel model;
    // Format of ListScenarios()
    model.setScenarios(
        uR"([{"name":"no_sound","title":"Pas de son","description":"Diagnose sound","icon":"audio-volume-muted","diagnosticTools":[]},
                             {"name":"custom","title":"","description":"","icon":""}])"_s);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(0).data(ScenarioModel::TitleRole).toString(), u"Pas de son"_s);
    QCOMPARE(model.index(0).data(ScenarioModel::DescriptionRole).toString(), u"Diagnose sound"_s);
    // Missing title and icon
    QCOMPARE(model.index(1).data(ScenarioModel::TitleRole).toString(), u"custom"_s);
    QVERIFY(!model.index(1).data(ScenarioModel::IconNameRole).toString().isEmpty());
    QCOMPARE(model.title(u"no_sound"_s), u"Pas de son"_s);

    // Invalid answer keeps scenarios
    model.setScenarios(u"[]"_s);
    QCOMPARE(model.rowCount(), 2);
}

#include "moc_scenariomodeltest.cpp"
