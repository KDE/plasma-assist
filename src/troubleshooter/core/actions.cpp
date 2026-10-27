/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "actions.h"
#include "privacyfilter.h"
#include "toolhelpers.h"
#include <KLocalizedString>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr auto pollInterval = 100ms;
constexpr int listedChanges = 20;

QJsonObject arguments(McpProtocolServerToolCall *call)
{
    QJsonObject result;
    const QMap<QString, QJsonValue> values = call->arguments();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        result.insert(it.key(), it.value());
    }
    // Language model sees pseudonyms of personal data: action needs real values
    return PrivacyFilter::instance()->restore(result).toObject();
}

QString toJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// Change is sent to language model: arguments are not useful and before/after are already in summary
QJsonObject changeToJson(const ChangeJournal::Change &change)
{
    return QJsonObject{
        {u"id"_s, change.identifier},
        {u"dateTime"_s, change.dateTime.toString(Qt::ISODate)},
        {u"tool"_s, change.tool},
        {u"summary"_s, change.summary},
        {u"undoable"_s, change.undoable && !change.undone},
        {u"undone"_s, change.undone},
    };
}

// Reads state until it's the expected state or timeout. Deleted with context (request cancelled)
class StatePoller : public QObject
{
public:
    StatePoller(const Action &action,
                const QJsonObject &arguments,
                const QJsonObject &expected,
                std::chrono::milliseconds timeout,
                const Action::StateCallback &callback,
                QObject *parent)
        : QObject(parent)
        , mAction(action)
        , mArguments(arguments)
        , mExpected(expected)
        , mTimeout(timeout)
        , mCallback(callback)
    {
        mTimer.start();
    }

    void poll()
    {
        mAction.readState(mArguments, this, [this](const QJsonObject &current, const QString &error) {
            if (!error.isEmpty() || current == mExpected || mTimer.durationElapsed() >= mTimeout) {
                deleteLater();
                mCallback(current, error);
                return;
            }
            QTimer::singleShot(pollInterval, this, &StatePoller::poll);
        });
    }

private:
    const Action mAction;
    const QJsonObject mArguments;
    const QJsonObject mExpected;
    const std::chrono::milliseconds mTimeout;
    const Action::StateCallback mCallback;
    QElapsedTimer mTimer;
};

void finish(McpProtocolServerToolCall *call, const QJsonObject &result, const QString &error)
{
    if (!error.isEmpty()) {
        ToolHelpers::finishWithError(call, error);
        return;
    }
    ToolHelpers::finishWithJson(call, result, result.value("summary"_L1).toString());
}
}

ActionRunner::ActionRunner(ChangeJournal *journal)
    : mJournal(journal)
    , mApplyTimeout(3s)
{
}

ActionRunner::~ActionRunner() = default;

void ActionRunner::setApplyTimeout(std::chrono::milliseconds timeout)
{
    mApplyTimeout = timeout;
}

ChangeJournal *ActionRunner::journal() const
{
    return mJournal;
}

void ActionRunner::addAction(const QString &tool, const Action &action)
{
    mActions.insert(tool, action);
}

void ActionRunner::addActionTool(McpProtocolServerProtocolManager *manager, const McpProtocolTool &definition, const Action &action)
{
    const QString tool = definition.name();
    addAction(tool, action);
    ToolHelpers::addFunctionTool(manager, definition, [this, tool](McpProtocolServerToolCall *call) {
        run(tool, arguments(call), call, [call](const QJsonObject &result, const QString &error) {
            finish(call, result, error);
        });
    });
}

void ActionRunner::record(const QString &tool, const QString &summary, const QJsonObject &arguments)
{
    (void)mJournal->add(tool, summary, arguments, {}, {}, false);
}

void ActionRunner::apply(const Action &action, const QJsonObject &arguments, const QJsonObject &state, QObject *context, const Action::StateCallback &callback)
{
    action.applyState(arguments, state, context, [this, action, arguments, state, context, callback](const QString &error) {
        if (!error.isEmpty()) {
            callback({}, error);
            return;
        }
        // System applies changes asynchronously (audio server, NetworkManager…): read state until it's changed
        auto poller = new StatePoller(action, arguments, state, mApplyTimeout, callback, context);
        poller->poll();
    });
}

void ActionRunner::run(const QString &tool, const QJsonObject &arguments, QObject *context, const ResultCallback &callback)
{
    const auto it = mActions.constFind(tool);
    if (it == mActions.cend()) {
        callback({}, u"Unknown action: %1"_s.arg(tool));
        return;
    }
    const Action action = *it;
    action.readState(arguments, context, [this, tool, action, arguments, context, callback](const QJsonObject &before, const QString &error) {
        if (!error.isEmpty()) {
            callback({}, error);
            return;
        }
        QString targetError;
        const QJsonObject target = action.targetState(arguments, before, &targetError);
        if (!targetError.isEmpty()) {
            callback({}, targetError);
            return;
        }
        if (target == before) {
            callback(QJsonObject{{u"changed"_s, false}, {u"summary"_s, u"Nothing to change: system already has this state"_s}, {u"state"_s, before}}, {});
            return;
        }
        apply(action, arguments, target, context, [this, tool, action, arguments, before, target, callback](const QJsonObject &after, const QString &error) {
            if (!error.isEmpty()) {
                callback({}, error);
                return;
            }
            if (after == before) {
                callback({}, u"Change was not applied: state is still %1"_s.arg(toJson(after)));
                return;
            }
            const ChangeJournal::Change change = mJournal->add(tool, action.summary(before, after), arguments, before, after, true);
            QJsonObject result{
                {u"changed"_s, true},
                {u"changeId"_s, change.identifier},
                {u"summary"_s, change.summary},
                {u"before"_s, before},
                {u"after"_s, after},
            };
            if (after != target) {
                result.insert(u"warning"_s, u"System state is different from requested state"_s);
            }
            callback(result, {});
        });
    });
}

void ActionRunner::undo(const QString &changeIdentifier, QObject *context, const ResultCallback &callback)
{
    const std::optional<ChangeJournal::Change> change = mJournal->change(changeIdentifier);
    if (!change) {
        callback({}, u"Unknown change: %1"_s.arg(changeIdentifier));
        return;
    }
    if (!change->undoable || change->undone) {
        callback({}, change->undone ? u"Change was already undone"_s : u"This change can't be undone"_s);
        return;
    }
    const auto it = mActions.constFind(change->tool);
    if (it == mActions.cend()) {
        callback({}, u"Unknown action: %1"_s.arg(change->tool));
        return;
    }
    const Action action = *it;
    const ChangeJournal::Change original = *change;
    action.readState(original.arguments, context, [this, action, original, context, callback](const QJsonObject &current, const QString &error) {
        if (!error.isEmpty()) {
            callback({}, error);
            return;
        }
        // Don't overwrite a change made later by user or another application
        if (current != original.after) {
            callback({}, u"System has changed since this change (current state: %1): undo refused"_s.arg(toJson(current)));
            return;
        }
        apply(action, original.arguments, original.before, context, [this, original, callback](const QJsonObject &state, const QString &error) {
            if (!error.isEmpty()) {
                callback({}, error);
                return;
            }
            if (state != original.before) {
                callback({}, u"Undo was not applied: state is %1"_s.arg(toJson(state)));
                return;
            }
            mJournal->setUndone(original.identifier);
            callback(QJsonObject{{u"changed"_s, true}, {u"changeId"_s, original.identifier}, {u"summary"_s, i18n("Undone: %1", original.summary)}}, {});
        });
    });
}

void ActionRunner::registerTools(McpProtocolServerProtocolManager *manager)
{
    const ToolHelpers::ToolGroup group{u"changes"_s, i18nc("@title tools group", "Changes")};

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"list_changes"_s,
                                                         i18nc("@title tool", "Changes"),
                                                         u"List last changes made by the assistant (most recent first), with their id and whether they "
                                                         "can still be undone with undo_change."_s,
                                                         ToolHelpers::ToolKind::Diagnostic,
                                                         group),
                                 [this](McpProtocolServerToolCall *call) {
                                     QJsonArray changes;
                                     const QList<ChangeJournal::Change> list = mJournal->changes();
                                     for (const ChangeJournal::Change &change : list) {
                                         if (changes.count() == listedChanges) {
                                             break;
                                         }
                                         changes.append(changeToJson(change));
                                     }
                                     ToolHelpers::finishWithJson(call, QJsonObject{{u"changes"_s, changes}}, u"%1 changes"_s.arg(list.count()));
                                 });

    ToolHelpers::addFunctionTool(manager,
                                 ToolHelpers::createTool(u"undo_change"_s,
                                                         i18nc("@title tool", "Undo Change"),
                                                         u"Undo a change listed by list_changes: previous state is restored. It's refused when system "
                                                         "has changed since."_s,
                                                         ToolHelpers::ToolKind::ReversibleAction,
                                                         group,
                                                         {{u"id"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, u"Id of change"_s}}}},
                                                         {u"id"_s}),
                                 [this](McpProtocolServerToolCall *call) {
                                     undo(call->arguments().value(u"id"_s).toString(), call, [call](const QJsonObject &result, const QString &error) {
                                         finish(call, result, error);
                                     });
                                 });
}
