/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "changejournaltest.h"
#include "core/changejournal.h"
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(ChangeJournalTest)

ChangeJournalTest::ChangeJournalTest(QObject *parent)
    : QObject{parent}
{
}

void ChangeJournalTest::shouldBeEmptyByDefault()
{
    const QTemporaryDir directory;
    const ChangeJournal journal(directory.filePath(u"changes.json"_s));
    QVERIFY(journal.changes().isEmpty());
    QVERIFY(!journal.change(u"unknown"_s));
}

void ChangeJournalTest::shouldAddChanges()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    const ChangeJournal::Change first = journal.add(u"set_audio_mute"_s, u"Speakers muted"_s, {}, {{u"muted"_s, false}}, {{u"muted"_s, true}}, true);
    const ChangeJournal::Change second = journal.add(u"restart_plasmashell"_s, u"Restarted"_s, {}, {}, {}, false);
    QVERIFY(!first.identifier.isEmpty());
    QVERIFY(first.identifier != second.identifier);
    // Most recent first
    QCOMPARE(journal.changes().first().identifier, second.identifier);
    QCOMPARE(journal.change(first.identifier)->after, (QJsonObject{{u"muted"_s, true}}));

    journal.setUndone(first.identifier);
    QVERIFY(journal.change(first.identifier)->undone);
    QVERIFY(!journal.change(second.identifier)->undone);
}

void ChangeJournalTest::shouldKeepChangesAfterRestart()
{
    const QTemporaryDir directory;
    const QString filePath = directory.filePath(u"plasma-assist/changes.json"_s);
    QString identifier;
    {
        ChangeJournal journal(filePath);
        identifier =
            journal.add(u"set_power_profile"_s, u"Power profile: balanced → performance"_s, {{u"profile"_s, u"performance"_s}}, {}, {}, true).identifier;
        journal.setUndone(identifier);
    }
    const ChangeJournal journal(filePath);
    QCOMPARE(journal.changes().count(), 1);
    const ChangeJournal::Change change = journal.changes().first();
    QCOMPARE(change.identifier, identifier);
    QCOMPARE(change.tool, u"set_power_profile"_s);
    QCOMPARE(change.arguments.value("profile"_L1).toString(), u"performance"_s);
    QVERIFY(change.dateTime.isValid());
    QVERIFY(change.undone);
}

void ChangeJournalTest::shouldLimitNumberOfChanges()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    for (int i = 0; i < ChangeJournal::maximumChanges + 5; ++i) {
        journal.add(u"tool"_s, QString::number(i), {}, {}, {}, true);
    }
    QCOMPARE(journal.changes().count(), ChangeJournal::maximumChanges);
    QCOMPARE(journal.changes().first().summary, QString::number(ChangeJournal::maximumChanges + 4));
}

#include "moc_changejournaltest.cpp"
