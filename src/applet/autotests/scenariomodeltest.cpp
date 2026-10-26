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
        QVERIFY(!index.data(ScenarioModel::PromptRole).toString().isEmpty());
    }
    QVERIFY(identifiers.contains(u"no_sound"_s));
    QCOMPARE(model.roleNames().value(ScenarioModel::PromptRole), "prompt"_ba);
}

#include "moc_scenariomodeltest.cpp"
