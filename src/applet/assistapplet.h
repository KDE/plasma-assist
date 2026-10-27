/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <Plasma/Applet>
#include <QAbstractListModel>

class ConversationModel;
class OrgKdePlasmaAssistAssistantInterface;
class QDBusServiceWatcher;
class ScenarioModel;

// Applet: user interface of plasma-assistd (D-Bus)
class AssistApplet : public Plasma::Applet
{
    Q_OBJECT
    Q_PROPERTY(bool daemonAvailable READ daemonAvailable NOTIFY daemonAvailableChanged)
    Q_PROPERTY(QString daemonStatus READ daemonStatus NOTIFY daemonStatusChanged)
    // Error of language model reported by daemon (not configured, remote model refused…)
    Q_PROPERTY(QString languageModelError READ languageModelError NOTIFY daemonStatusChanged)
    Q_PROPERTY(QAbstractListModel *scenarioModel READ scenarioModel CONSTANT)
    Q_PROPERTY(QAbstractListModel *conversationModel READ conversationModel CONSTANT)
    // Conversation page is shown
    Q_PROPERTY(bool conversationOpen READ conversationOpen NOTIFY conversationOpenChanged)
    // Waiting for answer of assistant
    Q_PROPERTY(bool answerInProgress READ answerInProgress NOTIFY answerInProgressChanged)
    // JSON objects: id, title, dateTime (most recent first)
    Q_PROPERTY(QVariantList recentConversations READ recentConversations NOTIFY recentConversationsChanged)
    // Assistant can change system (after a confirmation of user)
    Q_PROPERTY(bool actionsAllowed READ actionsAllowed NOTIFY daemonStatusChanged)
    // A confirmation waits for an answer of user
    Q_PROPERTY(bool confirmationPending READ confirmationPending NOTIFY confirmationPendingChanged)
    // JSON objects: id, dateTime, tool, summary, undoable, undone (most recent first)
    Q_PROPERTY(QVariantList changes READ changes NOTIFY changesChanged)
    // Error of last undo
    Q_PROPERTY(QString changesError READ changesError NOTIFY changesChanged)
public:
    AssistApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~AssistApplet() override;

    [[nodiscard]] bool daemonAvailable() const;
    // JSON status sent by daemon
    [[nodiscard]] QString daemonStatus() const;
    [[nodiscard]] QString languageModelError() const;
    [[nodiscard]] QAbstractListModel *scenarioModel() const;
    [[nodiscard]] QAbstractListModel *conversationModel() const;
    [[nodiscard]] bool conversationOpen() const;
    [[nodiscard]] bool answerInProgress() const;
    [[nodiscard]] QVariantList recentConversations() const;
    [[nodiscard]] bool actionsAllowed() const;
    [[nodiscard]] bool confirmationPending() const;
    [[nodiscard]] QVariantList changes() const;
    [[nodiscard]] QString changesError() const;

    // Starts daemon by D-Bus activation
    Q_INVOKABLE void startDaemon();
    // Sends \a text in current conversation, a conversation is created when none is open
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openConversation(const QString &identifier);
    // Back to home page
    Q_INVOKABLE void closeConversation();
    Q_INVOKABLE void refreshConversations();
    // \a answer: 0 deny, 1 allow, 2 always allow this tool
    Q_INVOKABLE void confirm(const QString &requestId, int answer);
    Q_INVOKABLE void setActionsAllowed(bool allowed);
    Q_INVOKABLE void refreshChanges();
    Q_INVOKABLE void undoChange(const QString &changeId);

Q_SIGNALS:
    void daemonAvailableChanged();
    void daemonStatusChanged();
    void conversationOpenChanged();
    void answerInProgressChanged();
    void recentConversationsChanged();
    void confirmationPendingChanged();
    void changesChanged();

private:
    void setDaemonAvailable(bool available);
    void setDaemonStatus(const QString &status);
    void fetchStatus();
    void setConversationOpen(bool open);
    void setAnswerInProgress(bool inProgress);
    void send(const QString &text);
    void showError(const QString &error);
    void updateConfirmationPending();

    OrgKdePlasmaAssistAssistantInterface *const mInterface;
    QDBusServiceWatcher *const mServiceWatcher;
    ScenarioModel *const mScenarioModel;
    ConversationModel *const mConversationModel;
    QString mDaemonStatus;
    QString mConversationId;
    QVariantList mRecentConversations;
    QVariantList mChanges;
    QString mChangesError;
    bool mDaemonAvailable = false;
    bool mConversationOpen = false;
    bool mAnswerInProgress = false;
    bool mConfirmationPending = false;
};
