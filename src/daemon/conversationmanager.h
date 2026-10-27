/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

class ConfirmationManager;

namespace TextAutoGenerateText
{
class TextAutoGenerateManager;
class TextAutoGenerateMessagesModel;
}

// Conversations with language model, which uses tools of troubleshooter server.
// Language model and conversations are loaded on first use.
class ConversationManager : public QObject
{
    Q_OBJECT
public:
    enum class EngineStatus : uint8_t {
        NotLoaded,
        Loading,
        Ready,
        Error,
    };
    Q_ENUM(EngineStatus)

    struct Conversation {
        QByteArray identifier;
        QString title;
        qint64 dateTime = -1;
    };

    explicit ConversationManager(QObject *parent = nullptr);
    ~ConversationManager() override;

    [[nodiscard]] EngineStatus engineStatus() const;
    [[nodiscard]] QString engineStatusName() const;
    [[nodiscard]] QString errorString() const;
    // Instance and model used, empty when language model is not loaded
    [[nodiscard]] QString engineName() const;
    [[nodiscard]] QString modelName() const;

    [[nodiscard]] QByteArray newConversation();
    [[nodiscard]] QList<Conversation> conversations();
    [[nodiscard]] bool hasConversation(const QByteArray &conversation);
    // Removes conversations from database, except those whose answer is in progress
    void clearConversations();
    // JSON array of messages: id, role ("user" or "assistant"), text, dateTime, inProgress, toolCalls
    [[nodiscard]] QJsonArray messages(const QByteArray &conversation);
    // Message is sent when language model is ready (errorOccurred() is emitted if it can't be loaded).
    // \a client: D-Bus client which sent message, only it can confirm actions of answer.
    // Returns error when conversation is unknown or busy.
    [[nodiscard]] QString sendMessage(const QByteArray &conversation, const QString &text, const QString &client = {});
    // Sends prompt of a scenario: names of tools in \a text are replaced by names known by language model
    // (errorOccurred() is emitted on error)
    void sendScenario(const QByteArray &conversation, const QString &text, const QString &client);
    bool cancel(const QByteArray &conversation);
    [[nodiscard]] bool hasConversationInProgress() const;

    // Asks user to confirm tools which change system (they are refused without it)
    void setConfirmationManager(ConfirmationManager *confirmationManager);

    // Opens dialog of ktextaddons to choose language model
    void showConfigureDialog();
    [[nodiscard]] bool isConfigureDialogVisible() const;

    // Language models which send data to a remote server are refused unless allowed in plasma-assistrc
    [[nodiscard]] static bool remoteLanguageModelAllowed();

Q_SIGNALS:
    void engineStatusChanged();
    void conversationsChanged();
    // \a text is the whole answer received so far
    void messageChunk(const QByteArray &conversation, const QByteArray &messageId, const QString &text);
    void messageFinished(const QByteArray &conversation, const QByteArray &messageId);
    void toolCallStarted(const QByteArray &conversation, const QString &callId, const QString &tool, const QString &argumentsJson);
    void toolCallFinished(const QByteArray &conversation, const QString &callId, const QString &result, bool isError);
    void errorOccurred(const QByteArray &conversation, const QString &error);

private:
    struct PendingMessage {
        QByteArray conversation;
        QString text;
    };
    void ensureManager();
    void loadEngine();
    void slotEngineLoaded();
    // Error which prevents to use current language model
    [[nodiscard]] QString engineError() const;
    // Error when no model can be used (known when language model is initialized)
    [[nodiscard]] QString modelError() const;
    void setEngineStatus(EngineStatus status, const QString &errorString = {});
    void send(const PendingMessage &message);
    void watchConversation(const QByteArray &conversation);
    void slotMessageChanged(const QByteArray &conversation, const QByteArray &messageId);
    void slotConversationInProgressChanged(const QByteArray &conversation, bool inProgress);
    void setTitleIfEmpty(const QByteArray &conversation, const QString &text);
    [[nodiscard]] QString toolName(const QString &exposedName) const;
    [[nodiscard]] TextAutoGenerateText::TextAutoGenerateMessagesModel *messagesModel(const QByteArray &conversation);

    TextAutoGenerateText::TextAutoGenerateManager *mManager = nullptr;
    QPointer<QDialog> mConfigureDialog;
    QTimer mEngineTimer;
    QList<PendingMessage> mPendingMessages;
    QSet<QByteArray> mWatchedConversations;
    QSet<QByteArray> mConversationsInProgress;
    QHash<QByteArray, QString> mConversationClients;
    ConfirmationManager *mConfirmationManager = nullptr;
    // Answer already sent with messageChunk() and number of tool exchange entries already announced, by message
    QHash<QByteArray, QString> mSentAnswers;
    QHash<QByteArray, qsizetype> mSentToolEntries;
    QString mErrorString;
    EngineStatus mEngineStatus = EngineStatus::NotLoaded;
};
