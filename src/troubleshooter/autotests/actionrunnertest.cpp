/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "actionrunnertest.h"
#include "core/actions.h"
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
QTEST_GUILESS_MAIN(ActionRunnerTest)

namespace
{
// Value of a fake system, changed by action "set_value"
struct FakeSystem {
    int value = 1;
    // Value is changed after a delay (asynchronous system)
    std::chrono::milliseconds delay = 0ms;
    // System ignores changes
    bool ignoreChanges = false;
};

Action valueAction(const std::shared_ptr<FakeSystem> &system)
{
    return Action{
        .readState =
            [system](const QJsonObject &, QObject *, const Action::StateCallback &callback) {
                callback(QJsonObject{{u"value"_s, system->value}}, {});
            },
        .targetState =
            [](const QJsonObject &arguments, const QJsonObject &, QString *error) {
                if (!arguments.contains("value"_L1)) {
                    *error = u"value is missing"_s;
                    return QJsonObject();
                }
                return QJsonObject{{u"value"_s, arguments.value("value"_L1).toInt()}};
            },
        .applyState =
            [system](const QJsonObject &, const QJsonObject &state, QObject *context, const Action::ErrorCallback &callback) {
                if (!system->ignoreChanges) {
                    const int value = state.value("value"_L1).toInt();
                    if (system->delay == 0ms) {
                        system->value = value;
                    } else {
                        QTimer::singleShot(system->delay, context, [system, value]() {
                            system->value = value;
                        });
                    }
                }
                callback({});
            },
        .summary =
            [](const QJsonObject &before, const QJsonObject &after) {
                return u"Value: %1 → %2"_s.arg(before.value("value"_L1).toInt()).arg(after.value("value"_L1).toInt());
            },
    };
}

struct Result {
    QJsonObject result;
    QString error;
    bool finished = false;
};

Result run(ActionRunner &runner, const QJsonObject &arguments)
{
    auto result = std::make_shared<Result>();
    QObject context;
    runner.run(u"set_value"_s, arguments, &context, [result](const QJsonObject &object, const QString &error) noexcept {
        result->result = object;
        result->error = error;
        result->finished = true;
    });
    (void)QTest::qWaitFor(
        [result]() {
            return result->finished;
        },
        5000);
    return *result;
}

Result undo(ActionRunner &runner, const QString &identifier)
{
    auto result = std::make_shared<Result>();
    QObject context;
    runner.undo(identifier, &context, [result](const QJsonObject &object, const QString &error) noexcept {
        result->result = object;
        result->error = error;
        result->finished = true;
    });
    (void)QTest::qWaitFor(
        [result]() {
            return result->finished;
        },
        5000);
    return *result;
}
}

ActionRunnerTest::ActionRunnerTest(QObject *parent)
    : QObject{parent}
{
}

void ActionRunnerTest::shouldApplyAndUndo()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    ActionRunner runner(&journal);
    auto system = std::make_shared<FakeSystem>();
    runner.addAction(u"set_value"_s, valueAction(system));

    const Result result = run(runner, {{u"value"_s, 5}});
    QVERIFY(result.finished);
    QVERIFY(result.error.isEmpty());
    QVERIFY(result.result.value("changed"_L1).toBool());
    QCOMPARE(result.result.value("summary"_L1).toString(), u"Value: 1 → 5"_s);
    QCOMPARE(system->value, 5);
    const QString identifier = result.result.value("changeId"_L1).toString();
    QCOMPARE(journal.changes().count(), 1);
    QCOMPARE(journal.change(identifier)->before.value("value"_L1).toInt(), 1);

    const Result undone = undo(runner, identifier);
    QVERIFY(undone.error.isEmpty());
    QCOMPARE(system->value, 1);
    QVERIFY(journal.change(identifier)->undone);
    // A change is undone only once
    QVERIFY(!undo(runner, identifier).error.isEmpty());
    QVERIFY(!undo(runner, u"unknown"_s).error.isEmpty());
}

void ActionRunnerTest::shouldNotChangeSameState()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    ActionRunner runner(&journal);
    runner.addAction(u"set_value"_s, valueAction(std::make_shared<FakeSystem>()));
    const Result result = run(runner, {{u"value"_s, 1}});
    QVERIFY(result.error.isEmpty());
    QVERIFY(!result.result.value("changed"_L1).toBool());
    QVERIFY(journal.changes().isEmpty());
}

void ActionRunnerTest::shouldRejectInvalidArguments()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    ActionRunner runner(&journal);
    runner.addAction(u"set_value"_s, valueAction(std::make_shared<FakeSystem>()));
    QCOMPARE(run(runner, {}).error, u"value is missing"_s);
    QVERIFY(journal.changes().isEmpty());
}

void ActionRunnerTest::shouldRefuseUndoWhenStateChanged()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    ActionRunner runner(&journal);
    auto system = std::make_shared<FakeSystem>();
    runner.addAction(u"set_value"_s, valueAction(system));
    const QString identifier = run(runner, {{u"value"_s, 5}}).result.value("changeId"_L1).toString();
    // User changed value since
    system->value = 7;
    QVERIFY(undo(runner, identifier).error.contains("undo refused"_L1));
    QCOMPARE(system->value, 7);
    QVERIFY(!journal.change(identifier)->undone);
}

void ActionRunnerTest::shouldReportChangeNotApplied()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    ActionRunner runner(&journal);
    runner.setApplyTimeout(300ms);
    auto system = std::make_shared<FakeSystem>();
    system->ignoreChanges = true;
    runner.addAction(u"set_value"_s, valueAction(system));
    QVERIFY(run(runner, {{u"value"_s, 5}}).error.contains("not applied"_L1));
    QVERIFY(journal.changes().isEmpty());
}

void ActionRunnerTest::shouldWaitForAsynchronousChange()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    ActionRunner runner(&journal);
    auto system = std::make_shared<FakeSystem>();
    system->delay = 250ms;
    runner.addAction(u"set_value"_s, valueAction(system));
    const Result result = run(runner, {{u"value"_s, 3}});
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.result.value("after"_L1).toObject().value("value"_L1).toInt(), 3);
    QVERIFY(!result.result.contains("warning"_L1));
}

void ActionRunnerTest::shouldRecordActionWithoutState()
{
    const QTemporaryDir directory;
    ChangeJournal journal(directory.filePath(u"changes.json"_s));
    ActionRunner runner(&journal);
    runner.record(u"restart_plasmashell"_s, u"Plasma shell restarted"_s, {});
    QCOMPARE(journal.changes().count(), 1);
    QVERIFY(!journal.changes().first().undoable);
    QVERIFY(undo(runner, journal.changes().first().identifier).error.contains("can't be undone"_L1));
}

#include "moc_actionrunnertest.cpp"
