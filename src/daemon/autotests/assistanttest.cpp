/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "assistanttest.h"
#include "assistant.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(AssistantTest)

namespace
{
QJsonObject statusObject(const QString &status)
{
    return QJsonDocument::fromJson(status.toUtf8()).object();
}
}

AssistantTest::AssistantTest(QObject *parent)
    : QObject{parent}
{
}

void AssistantTest::shouldHaveDefaultStatus()
{
    const Assistant assistant;
    const QJsonObject status = statusObject(assistant.Status());
    QVERIFY(!status.value("version"_L1).toString().isEmpty());
    QVERIFY(!status.value("ready"_L1).toBool());
    QCOMPARE(status.value("conversations"_L1).toInt(), 0);
}

void AssistantTest::shouldCreateConversation()
{
    Assistant assistant;
    QSignalSpy statusChanged(&assistant, &Assistant::StatusChanged);
    const QString first = assistant.NewConversation();
    const QString second = assistant.NewConversation();
    QVERIFY(!first.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(statusChanged.count(), 2);
    QCOMPARE(statusObject(statusChanged.last().at(0).toString()).value("conversations"_L1).toInt(), 2);
}

#include "moc_assistanttest.cpp"
