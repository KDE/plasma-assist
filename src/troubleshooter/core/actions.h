/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "changejournal.h"
#include <QHash>
#include <QJsonObject>
#include <chrono>
#include <functional>

class QObject;
namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
class McpProtocolTool;
}

// Action which changes a state of system. Runner reads state before, applies target state, waits until system has it,
// and saves change in journal: undo_change applies state before when current state is still state after.
struct Action {
    using StateCallback = std::function<void(const QJsonObject &state, const QString &error)>;
    using ErrorCallback = std::function<void(const QString &error)>;

    // Reads state changed by action for \a arguments, e.g. {"default": "alsa_output…"}
    std::function<void(const QJsonObject &arguments, QObject *context, const StateCallback &callback)> readState;
    // State wanted by \a arguments: returns error when arguments are invalid (unknown device…)
    std::function<QJsonObject(const QJsonObject &arguments, const QJsonObject &current, QString *error)> targetState;
    std::function<void(const QJsonObject &arguments, const QJsonObject &state, QObject *context, const ErrorCallback &callback)> applyState;
    // Translated summary, e.g. "Default output: HDMI → Headphones"
    std::function<QString(const QJsonObject &before, const QJsonObject &after)> summary;
};

// Runs actions and records them in a journal. Provides tools list_changes and undo_change
class ActionRunner
{
public:
    using ResultCallback = std::function<void(const QJsonObject &result, const QString &error)>;

    explicit ActionRunner(ChangeJournal *journal);
    ~ActionRunner();

    // Time to wait until system has target state
    void setApplyTimeout(std::chrono::milliseconds timeout);

    // Action without tool (tests)
    void addAction(const QString &tool, const Action &action);
    // Registers tool \a definition which runs \a action
    void addActionTool(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager,
                       const TextAutoGenerateTextMcpProtocolCore::McpProtocolTool &definition,
                       const Action &action);
    // Records an action which has no state (restart, reconnection): it can't be undone
    void record(const QString &tool, const QString &summary, const QJsonObject &arguments);

    void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager);

    // Result: changed, changeId, summary, before, after
    void run(const QString &tool, const QJsonObject &arguments, QObject *context, const ResultCallback &callback);
    void undo(const QString &changeIdentifier, QObject *context, const ResultCallback &callback);

    [[nodiscard]] ChangeJournal *journal() const;

private:
    // Applies \a state and reads state until it's \a state (or timeout): callback receives state read
    void apply(const Action &action, const QJsonObject &arguments, const QJsonObject &state, QObject *context, const Action::StateCallback &callback);

    ChangeJournal *const mJournal;
    QHash<QString, Action> mActions;
    std::chrono::milliseconds mApplyTimeout;
};
