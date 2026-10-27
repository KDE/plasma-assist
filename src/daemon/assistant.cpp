/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "assistant.h"
#include "assistantadaptor.h"
#include "config-plasma-assist.h"
#include "confirmationmanager.h"
#include "conversationmanager.h"
#include "plasmaassist_daemon_debug.h"
#include "troubleshooterclient.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace
{
QString toJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// Same format as a tool result: client has only one case to handle
QJsonObject errorResult(const QString &message)
{
    return QJsonObject{
        {u"isError"_s, true},
        {u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, message}}}},
    };
}
}

Assistant::Assistant(QObject *parent)
    : QObject{parent}
    , mTroubleshooterClient(new TroubleshooterClient(this))
    , mConversationManager(new ConversationManager(this))
    , mConfirmationManager(new ConfirmationManager(this))
{
    new AssistantAdaptor(this);
    mConversationManager->setConfirmationManager(mConfirmationManager);
    connect(mConfirmationManager,
            &ConfirmationManager::confirmationRequested,
            this,
            [this](const QString &requestId,
                   const QByteArray &conversation,
                   const QString &tool,
                   const QString &summary,
                   const QString &argumentsJson,
                   const QString &risk) {
                Q_EMIT ConfirmationRequested(requestId, QString::fromLatin1(conversation), tool, summary, argumentsJson, risk);
            });
    connect(mConfirmationManager, &ConfirmationManager::confirmationFinished, this, &Assistant::ConfirmationFinished);
    connect(mConfirmationManager, &ConfirmationManager::actionRefused, this, [this](const QByteArray &conversation, const QString &title) {
        Q_EMIT ConversationNotice(QString::fromLatin1(conversation), i18n("The assistant wanted to use \"%1\", but actions are disabled in settings.", title));
    });
    connect(mConfirmationManager, &ConfirmationManager::actionsAllowedChanged, this, [this]() {
        Q_EMIT StatusChanged(Status());
    });
    const auto emitStatusChanged = [this]() {
        Q_EMIT StatusChanged(Status());
    };
    connect(mTroubleshooterClient, &TroubleshooterClient::statusChanged, this, emitStatusChanged);
    connect(mConversationManager, &ConversationManager::engineStatusChanged, this, emitStatusChanged);
    connect(mConversationManager, &ConversationManager::conversationsChanged, this, &Assistant::ConversationsChanged);
    connect(mConversationManager,
            &ConversationManager::messageChunk,
            this,
            [this](const QByteArray &conversation, const QByteArray &messageId, const QString &text) {
                restartIdleTimer();
                Q_EMIT MessageChunk(QString::fromLatin1(conversation), QString::fromLatin1(messageId), text);
            });
    connect(mConversationManager, &ConversationManager::messageFinished, this, [this](const QByteArray &conversation, const QByteArray &messageId) {
        restartIdleTimer();
        Q_EMIT MessageFinished(QString::fromLatin1(conversation), QString::fromLatin1(messageId));
    });
    connect(mConversationManager,
            &ConversationManager::toolCallStarted,
            this,
            [this](const QByteArray &conversation, const QString &callId, const QString &tool, const QString &argumentsJson) {
                mToolCalls.insert(callId, tool);
                Q_EMIT ToolCallStarted(QString::fromLatin1(conversation), callId, tool, argumentsJson);
            });
    connect(mConversationManager,
            &ConversationManager::toolCallFinished,
            this,
            [this](const QByteArray &conversation, const QString &callId, const QString &result, bool isError) {
                Q_EMIT ToolCallFinished(QString::fromLatin1(conversation), callId, result, isError);
                if (!isReadOnlyTool(mToolCalls.take(callId))) {
                    Q_EMIT ChangesChanged();
                }
            });
    connect(mConversationManager, &ConversationManager::errorOccurred, this, [this](const QByteArray &conversation, const QString &error) {
        Q_EMIT ConversationError(QString::fromLatin1(conversation), error);
    });
    mIdleTimer.setSingleShot(true);
    mIdleTimer.setInterval(10min);
    connect(&mIdleTimer, &QTimer::timeout, this, &Assistant::slotIdleTimeout);
    mIdleTimer.start();
}

Assistant::~Assistant() = default;

bool Assistant::registerOnSessionBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(u"/Assistant"_s, this)) {
        qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Impossible to register object:" << bus.lastError().message();
        return false;
    }
    if (!bus.registerService(u"org.kde.plasma.assist"_s)) {
        qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Impossible to register service (already running?):" << bus.lastError().message();
        return false;
    }
    return true;
}

TroubleshooterClient *Assistant::troubleshooterClient() const
{
    return mTroubleshooterClient;
}

ConversationManager *Assistant::conversationManager() const
{
    return mConversationManager;
}

ConfirmationManager *Assistant::confirmationManager() const
{
    return mConfirmationManager;
}

QString Assistant::Status() const
{
    const QJsonObject status{
        {u"version"_s, QStringLiteral(PLASMA_ASSIST_VERSION)},
        {u"ready"_s, mConversationManager->engineStatus() == ConversationManager::EngineStatus::Ready},
        {u"actionsAllowed"_s, mConfirmationManager->actionsAllowed()},
        {u"languageModel"_s,
         QJsonObject{
             {u"status"_s, mConversationManager->engineStatusName()},
             {u"error"_s, mConversationManager->errorString()},
             {u"name"_s, mConversationManager->engineName()},
             {u"model"_s, mConversationManager->modelName()},
         }},
        {u"troubleshooter"_s,
         QJsonObject{
             {u"status"_s, mTroubleshooterClient->statusName()},
             {u"error"_s, mTroubleshooterClient->errorString()},
             {u"tools"_s, mTroubleshooterClient->tools().count()},
         }},
    };
    return toJson(status);
}

QString Assistant::NewConversation()
{
    restartIdleTimer();
    return QString::fromLatin1(mConversationManager->newConversation());
}

QString Assistant::ListConversations()
{
    restartIdleTimer();
    QJsonArray conversations;
    const QList<ConversationManager::Conversation> list = mConversationManager->conversations();
    for (const ConversationManager::Conversation &conversation : list) {
        conversations.append(QJsonObject{
            {u"id"_s, QString::fromLatin1(conversation.identifier)},
            {u"title"_s, conversation.title},
            {u"dateTime"_s, QDateTime::fromSecsSinceEpoch(conversation.dateTime).toString(Qt::ISODate)},
        });
    }
    return QString::fromUtf8(QJsonDocument(conversations).toJson(QJsonDocument::Compact));
}

QString Assistant::GetMessages(const QString &conversation)
{
    restartIdleTimer();
    if (!mConversationManager->hasConversation(conversation.toLatin1())) {
        if (calledFromDBus()) {
            sendErrorReply(QDBusError::InvalidArgs, u"Unknown conversation: %1"_s.arg(conversation));
        }
        return {};
    }
    return QString::fromUtf8(QJsonDocument(mConversationManager->messages(conversation.toLatin1())).toJson(QJsonDocument::Compact));
}

void Assistant::SendMessage(const QString &conversation, const QString &text)
{
    restartIdleTimer();
    // Only client which sent message can confirm actions
    const QString client = calledFromDBus() ? message().service() : QString();
    if (const QString error = mConversationManager->sendMessage(conversation.toLatin1(), text, client); !error.isEmpty() && calledFromDBus()) {
        sendErrorReply(QDBusError::Failed, error);
    }
}

void Assistant::Cancel(const QString &conversation)
{
    restartIdleTimer();
    mConversationManager->cancel(conversation.toLatin1());
}

QString Assistant::ListTools()
{
    restartIdleTimer();
    if (mTroubleshooterClient->status() == TroubleshooterClient::Status::Ready || !calledFromDBus()) {
        return QString::fromUtf8(QJsonDocument(mTroubleshooterClient->tools()).toJson(QJsonDocument::Compact));
    }
    // Server is started on demand: answer when tools are known
    setDelayedReply(true);
    mTroubleshooterClient->whenReady(this, [this, request = message(), bus = connection()](const QString &error) {
        if (!error.isEmpty()) {
            bus.send(request.createErrorReply(QDBusError::Failed, error));
            return;
        }
        bus.send(request.createReply(QString::fromUtf8(QJsonDocument(mTroubleshooterClient->tools()).toJson(QJsonDocument::Compact))));
    });
    return {};
}

QString Assistant::CallTool(const QString &name, const QString &argumentsJson)
{
    if (!calledFromDBus()) {
        return toJson(errorResult(u"CallTool must be called by D-Bus, use callTool()"_s));
    }
    setDelayedReply(true);
    callTool(name, argumentsJson, this, [request = message(), bus = connection()](const QString &result) {
        bus.send(request.createReply(result));
    });
    return {};
}

void Assistant::callTool(const QString &name, const QString &argumentsJson, QObject *context, const std::function<void(const QString &result)> &callback)
{
    restartIdleTimer();
    QJsonParseError parseError;
    const QJsonDocument arguments =
        argumentsJson.trimmed().isEmpty() ? QJsonDocument(QJsonObject{}) : QJsonDocument::fromJson(argumentsJson.toUtf8(), &parseError);
    if (!arguments.isObject()) {
        callback(toJson(errorResult(u"Arguments must be a JSON object: %1"_s.arg(parseError.errorString()))));
        return;
    }
    mTroubleshooterClient->whenReady(context, [this, name, arguments = arguments.object(), context, callback](const QString &error) {
        if (!error.isEmpty()) {
            callback(toJson(errorResult(error)));
            return;
        }
        // Any process of session can call this method: it can't change system without confirmation of user
        if (!isReadOnlyTool(name)) {
            callback(toJson(errorResult(u"Tool %1 changes system: only the assistant can call it, after a confirmation of user"_s.arg(name))));
            return;
        }
        callServerTool(name, arguments, context, [callback](const QJsonObject &result) {
            callback(toJson(result));
        });
    });
}

void Assistant::callServerTool(const QString &name,
                               const QJsonObject &arguments,
                               QObject *context,
                               const std::function<void(const QJsonObject &result)> &callback)
{
    restartIdleTimer();
    mTroubleshooterClient->callTool(name, arguments, context, [callback](const QJsonObject &answer) {
        if (const QJsonValue result = answer.value("result"_L1); result.isObject()) {
            callback(result.toObject());
        } else {
            callback(errorResult(answer.value("error"_L1).toObject().value("message"_L1).toString()));
        }
    });
}

bool Assistant::isReadOnlyTool(const QString &name) const
{
    const QJsonArray tools = mTroubleshooterClient->tools();
    for (const QJsonValue &value : tools) {
        const QJsonObject tool = value.toObject();
        if (tool.value("name"_L1).toString() == name) {
            return tool.value("annotations"_L1).toObject().value("readOnlyHint"_L1).toBool();
        }
    }
    return true;
}

bool Assistant::Confirm(const QString &requestId, int answer)
{
    restartIdleTimer();
    if (answer < static_cast<int>(ConfirmationManager::Answer::Deny) || answer > static_cast<int>(ConfirmationManager::Answer::AlwaysAllow)) {
        return false;
    }
    return mConfirmationManager->confirm(requestId, static_cast<ConfirmationManager::Answer>(answer), calledFromDBus() ? message().service() : QString());
}

void Assistant::SetActionsAllowed(bool allowed)
{
    mConfirmationManager->setActionsAllowed(allowed);
}

QString Assistant::ListChanges()
{
    restartIdleTimer();
    if (!calledFromDBus()) {
        return {};
    }
    setDelayedReply(true);
    callServerTool(u"list_changes"_s, {}, this, [request = message(), bus = connection()](const QJsonObject &result) {
        if (result.value("isError"_L1).toBool()) {
            bus.send(request.createErrorReply(QDBusError::Failed, result.value("content"_L1).toArray().first().toObject().value("text"_L1).toString()));
            return;
        }
        const QJsonArray changes = result.value("structuredContent"_L1).toObject().value("changes"_L1).toArray();
        bus.send(request.createReply(QString::fromUtf8(QJsonDocument(changes).toJson(QJsonDocument::Compact))));
    });
    return {};
}

QString Assistant::UndoChange(const QString &changeId)
{
    restartIdleTimer();
    if (!calledFromDBus()) {
        return {};
    }
    // User asks it in user interface: no confirmation, undo is refused when system has changed since
    setDelayedReply(true);
    callServerTool(u"undo_change"_s, {{u"id"_s, changeId}}, this, [this, request = message(), bus = connection()](const QJsonObject &result) {
        bus.send(request.createReply(toJson(result)));
        Q_EMIT ChangesChanged();
    });
    return {};
}

void Assistant::restartIdleTimer()
{
    mIdleTimer.start();
}

void Assistant::slotIdleTimeout()
{
    if (mTroubleshooterClient->hasPendingRequest() || mConversationManager->hasConversationInProgress() || mConfirmationManager->hasPendingRequest()) {
        restartIdleTimer();
        return;
    }
    qCDebug(PLASMA_ASSIST_DAEMON_LOG) << "Daemon is not used anymore";
    Q_EMIT idleTimeout();
}

#include "moc_assistant.cpp"
