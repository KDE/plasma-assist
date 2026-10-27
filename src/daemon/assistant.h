/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDBusContext>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>

class ConfirmationManager;
class ConversationManager;
class TroubleshooterClient;

// Object exported on D-Bus (org.kde.plasma.assist.Assistant)
class Assistant : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    explicit Assistant(QObject *parent = nullptr);
    ~Assistant() override;

    // Exports object and requests service name on session bus
    [[nodiscard]] bool registerOnSessionBus();

    [[nodiscard]] TroubleshooterClient *troubleshooterClient() const;
    [[nodiscard]] ConversationManager *conversationManager() const;
    [[nodiscard]] ConfirmationManager *confirmationManager() const;

    // Calls tool without language model (only tools which don't change system).
    // \a callback receives JSON result ("isError" is true on error)
    void callTool(const QString &name, const QString &argumentsJson, QObject *context, const std::function<void(const QString &result)> &callback);

    // D-Bus API
    [[nodiscard]] QString Status() const;
    [[nodiscard]] QString NewConversation();
    [[nodiscard]] QString ListConversations();
    void ClearConversations();
    [[nodiscard]] QString GetMessages(const QString &conversation);
    void SendMessage(const QString &conversation, const QString &text);
    void Cancel(const QString &conversation);
    [[nodiscard]] QString ListTools();
    QString CallTool(const QString &name, const QString &argumentsJson);
    bool Confirm(const QString &requestId, int answer);
    void SetActionsAllowed(bool allowed);
    QString ListChanges();
    QString ListScenarios();
    void ShowConfigureDialog();
    void StartScenario(const QString &conversation, const QString &scenario, const QString &details);
    QString QuickDiagnostic(const QString &scenario);
    QString UndoChange(const QString &changeId);

Q_SIGNALS:
    void StatusChanged(const QString &status);
    void ConversationsChanged();
    void MessageChunk(const QString &conversation, const QString &messageId, const QString &text);
    void MessageFinished(const QString &conversation, const QString &messageId);
    void ToolCallStarted(const QString &conversation, const QString &callId, const QString &tool, const QString &argumentsJson);
    void ToolCallFinished(const QString &conversation, const QString &callId, const QString &result, bool isError);
    void ConversationError(const QString &conversation, const QString &error);
    void ConversationNotice(const QString &conversation, const QString &text);
    void ConfirmationRequested(const QString &requestId,
                               const QString &conversation,
                               const QString &tool,
                               const QString &summary,
                               const QString &argumentsJson,
                               const QString &risk);
    void ConfirmationFinished(const QString &requestId, bool allowed);
    void ChangesChanged();
    // Daemon is not used anymore: application can quit
    void idleTimeout();

private:
    // Calls tool of server without checking it: \a callback receives MCP result
    void callServerTool(const QString &name, const QJsonObject &arguments, QObject *context, const std::function<void(const QJsonObject &result)> &callback);
    // Unknown tools are considered as read only: server answers with an error
    [[nodiscard]] bool isReadOnlyTool(const QString &name) const;
    [[nodiscard]] QString toolTitle(const QString &name) const;
    // Read-only tools of scenario called without language model
    [[nodiscard]] QStringList diagnosticTools(const QString &scenario) const;
    // Calls \a tools one after another and adds their whole results (for language model) to \a results
    void collectToolResults(const QStringList &tools, const QString &results, const std::function<void(const QString &results)> &callback);
    // Calls \a tools one after another and adds their summaries to \a markdown
    void runQuickDiagnostic(const QStringList &tools, const QString &markdown, const std::function<void(const QString &markdown)> &callback);
    void restartIdleTimer();
    void slotIdleTimeout();

    TroubleshooterClient *const mTroubleshooterClient;
    ConversationManager *const mConversationManager;
    ConfirmationManager *const mConfirmationManager;
    // Tools called by language model, by call id: changes are announced when actions are finished
    QHash<QString, QString> mToolCalls;
    QTimer mIdleTimer;
};
