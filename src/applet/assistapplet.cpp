/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "assistapplet.h"
#include "assistantinterface.h"
#include "conversationmodel.h"
#include "plasmaassist_applet_debug.h"
#include "scenariomodel.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <type_traits>

using namespace Qt::Literals::StringLiterals;
namespace
{
constexpr QLatin1StringView serviceName{"org.kde.plasma.assist"};
constexpr int maximumRecentConversations = 5;

// Calls \a callback with reply of \a call, or logs error
template<typename... T>
void onReply(QObject *context, const QDBusPendingCall &call, const std::type_identity_t<std::function<void(const QDBusPendingReply<T...> &reply)>> &callback)
{
    auto watcher = new QDBusPendingCallWatcher(call, context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [callback](QDBusPendingCallWatcher *watcher) {
        callback(*watcher);
        watcher->deleteLater();
    });
}
}

AssistApplet::AssistApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , mInterface(new OrgKdePlasmaAssistAssistantInterface(serviceName, u"/Assistant"_s, QDBusConnection::sessionBus(), this))
    , mServiceWatcher(new QDBusServiceWatcher(serviceName, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this))
    , mScenarioModel(new ScenarioModel(this))
    , mConversationModel(new ConversationModel(this))
{
    // Language model can take several minutes to answer when it's loaded
    mInterface->setTimeout(-1);
    connect(mServiceWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        setDaemonAvailable(true);
    });
    connect(mServiceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        setDaemonAvailable(false);
    });
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::StatusChanged, this, &AssistApplet::setDaemonStatus);
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::ConversationsChanged, this, &AssistApplet::refreshConversations);
    connect(mInterface,
            &OrgKdePlasmaAssistAssistantInterface::MessageChunk,
            this,
            [this](const QString &conversation, const QString &messageId, const QString &text) {
                if (conversation == mConversationId) {
                    mConversationModel->setAnswer(messageId, text);
                }
            });
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::MessageFinished, this, [this](const QString &conversation) {
        if (conversation == mConversationId) {
            setAnswerInProgress(false);
        }
    });
    connect(mInterface,
            &OrgKdePlasmaAssistAssistantInterface::ToolCallStarted,
            this,
            [this](const QString &conversation, const QString &callId, const QString &tool, const QString &arguments) {
                if (conversation == mConversationId) {
                    mConversationModel->startToolCall(callId, tool, arguments);
                }
            });
    connect(mInterface,
            &OrgKdePlasmaAssistAssistantInterface::ToolCallFinished,
            this,
            [this](const QString &conversation, const QString &callId, const QString &result, bool isError) {
                if (conversation == mConversationId) {
                    mConversationModel->finishToolCall(callId, result, isError);
                }
            });
    connect(mInterface,
            &OrgKdePlasmaAssistAssistantInterface::ConfirmationRequested,
            this,
            [this](const QString &requestId,
                   const QString &conversation,
                   const QString &tool,
                   const QString &summary,
                   const QString &arguments,
                   const QString &risk) {
                if (conversation == mConversationId) {
                    mConversationModel->addConfirmation(requestId, tool, summary, arguments, risk);
                    updateConfirmationPending();
                }
            });
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::ConfirmationFinished, this, [this](const QString &requestId, bool allowed) {
        mConversationModel->finishConfirmation(requestId, allowed);
        updateConfirmationPending();
    });
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::ConversationNotice, this, [this](const QString &conversation, const QString &text) {
        if (conversation == mConversationId) {
            mConversationModel->addNotice(text);
        }
    });
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::ChangesChanged, this, &AssistApplet::refreshChanges);
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::ConversationError, this, [this](const QString &conversation, const QString &error) {
        if (conversation == mConversationId) {
            showError(error);
        }
    });
    setDaemonAvailable(QDBusConnection::sessionBus().interface()->isServiceRegistered(serviceName));
}

AssistApplet::~AssistApplet() = default;

bool AssistApplet::daemonAvailable() const
{
    return mDaemonAvailable;
}

QString AssistApplet::daemonStatus() const
{
    return mDaemonStatus;
}

QString AssistApplet::languageModelError() const
{
    return QJsonDocument::fromJson(mDaemonStatus.toUtf8()).object().value("languageModel"_L1).toObject().value("error"_L1).toString();
}

QAbstractListModel *AssistApplet::scenarioModel() const
{
    return mScenarioModel;
}

QAbstractListModel *AssistApplet::conversationModel() const
{
    return mConversationModel;
}

bool AssistApplet::conversationOpen() const
{
    return mConversationOpen;
}

bool AssistApplet::answerInProgress() const
{
    return mAnswerInProgress;
}

QVariantList AssistApplet::recentConversations() const
{
    return mRecentConversations;
}

bool AssistApplet::actionsAllowed() const
{
    return QJsonDocument::fromJson(mDaemonStatus.toUtf8()).object().value("actionsAllowed"_L1).toBool();
}

bool AssistApplet::confirmationPending() const
{
    return mConfirmationPending;
}

QVariantList AssistApplet::changes() const
{
    return mChanges;
}

QString AssistApplet::changesError() const
{
    return mChangesError;
}

void AssistApplet::confirm(const QString &requestId, int answer)
{
    // Daemon answers with ConfirmationFinished
    onReply<bool>(this, mInterface->Confirm(requestId, answer), [](const QDBusPendingReply<bool> &reply) {
        if (reply.isError() || !reply.value()) {
            qCWarning(PLASMA_ASSIST_APPLET_LOG) << "Confirmation was not accepted:" << reply.error().message();
        }
    });
}

void AssistApplet::setActionsAllowed(bool allowed)
{
    // Daemon sends new status
    mInterface->SetActionsAllowed(allowed);
}

void AssistApplet::refreshChanges()
{
    onReply<QString>(this, mInterface->ListChanges(), [this](const QDBusPendingReply<QString> &reply) {
        if (reply.isError()) {
            qCWarning(PLASMA_ASSIST_APPLET_LOG) << "Impossible to list changes:" << reply.error().message();
            return;
        }
        mChanges = QJsonDocument::fromJson(reply.value().toUtf8()).array().toVariantList();
        Q_EMIT changesChanged();
    });
}

void AssistApplet::undoChange(const QString &changeId)
{
    onReply<QString>(this, mInterface->UndoChange(changeId), [this](const QDBusPendingReply<QString> &reply) {
        const QJsonObject result = QJsonDocument::fromJson(reply.value().toUtf8()).object();
        if (reply.isError()) {
            mChangesError = reply.error().message();
        } else if (result.value("isError"_L1).toBool()) {
            mChangesError = result.value("content"_L1).toArray().first().toObject().value("text"_L1).toString();
        } else {
            mChangesError.clear();
        }
        // Daemon sends ChangesChanged: list is refreshed
        Q_EMIT changesChanged();
    });
}

void AssistApplet::updateConfirmationPending()
{
    if (const bool pending = mConversationModel->hasPendingConfirmation(); pending != mConfirmationPending) {
        mConfirmationPending = pending;
        Q_EMIT confirmationPendingChanged();
    }
}

void AssistApplet::startDaemon()
{
    if (mDaemonAvailable) {
        return;
    }
    // Async: serviceRegistered will be emitted when daemon is started
    onReply<uint>(this,
                  QDBusConnection::sessionBus().interface()->asyncCall(u"StartServiceByName"_s, serviceName, 0U),
                  [](const QDBusPendingReply<uint> &reply) {
                      if (reply.isError()) {
                          qCWarning(PLASMA_ASSIST_APPLET_LOG) << "Impossible to start daemon:" << reply.error().message();
                      }
                  });
}

void AssistApplet::sendMessage(const QString &text)
{
    const QString message = text.trimmed();
    if (message.isEmpty() || mAnswerInProgress) {
        return;
    }
    if (!mConversationOpen) {
        mConversationId.clear();
        mConversationModel->clear();
        setConversationOpen(true);
    }
    mConversationModel->addUserMessage(message);
    setAnswerInProgress(true);
    if (!mConversationId.isEmpty()) {
        send(message);
        return;
    }
    // Daemon is started by D-Bus activation if needed
    onReply<QString>(this, mInterface->NewConversation(), [this, message](const QDBusPendingReply<QString> &reply) {
        if (reply.isError() || reply.value().isEmpty()) {
            showError(i18n("Impossible to start a conversation: %1", reply.error().message()));
            return;
        }
        mConversationId = reply.value();
        send(message);
    });
}

void AssistApplet::send(const QString &text)
{
    onReply<>(this, mInterface->SendMessage(mConversationId, text), [this](const QDBusPendingReply<> &reply) {
        if (reply.isError()) {
            showError(reply.error().message());
        }
    });
}

void AssistApplet::cancel()
{
    if (!mConversationId.isEmpty()) {
        // Daemon answers with MessageFinished
        mInterface->Cancel(mConversationId);
    }
}

void AssistApplet::openConversation(const QString &identifier)
{
    mConversationId = identifier;
    mConversationModel->clear();
    setAnswerInProgress(false);
    setConversationOpen(true);
    onReply<QString>(this, mInterface->GetMessages(identifier), [this, identifier](const QDBusPendingReply<QString> &reply) {
        if (identifier != mConversationId) {
            return;
        }
        if (reply.isError()) {
            showError(reply.error().message());
            return;
        }
        mConversationModel->setMessages(reply.value());
    });
}

void AssistApplet::closeConversation()
{
    // Answer in progress continues in daemon, it's shown when conversation is opened again.
    // Confirmations are denied after a timeout
    mConversationId.clear();
    mConversationModel->clear();
    updateConfirmationPending();
    setAnswerInProgress(false);
    setConversationOpen(false);
}

void AssistApplet::refreshConversations()
{
    if (!mDaemonAvailable) {
        return;
    }
    onReply<QString>(this, mInterface->ListConversations(), [this](const QDBusPendingReply<QString> &reply) {
        if (reply.isError()) {
            qCWarning(PLASMA_ASSIST_APPLET_LOG) << "Impossible to list conversations:" << reply.error().message();
            return;
        }
        QVariantList conversations = QJsonDocument::fromJson(reply.value().toUtf8()).array().toVariantList();
        // Empty conversations have no title
        conversations.removeIf([](const QVariant &conversation) {
            return conversation.toMap().value(u"title"_s).toString().isEmpty();
        });
        std::ranges::sort(conversations, [](const QVariant &left, const QVariant &right) {
            return left.toMap().value(u"dateTime"_s).toString() > right.toMap().value(u"dateTime"_s).toString();
        });
        mRecentConversations = conversations.mid(0, maximumRecentConversations);
        Q_EMIT recentConversationsChanged();
    });
}

void AssistApplet::setDaemonAvailable(bool available)
{
    if (mDaemonAvailable == available) {
        return;
    }
    mDaemonAvailable = available;
    Q_EMIT daemonAvailableChanged();
    if (mDaemonAvailable) {
        fetchStatus();
        refreshConversations();
    } else {
        setDaemonStatus({});
        if (mAnswerInProgress) {
            showError(i18n("Assistant service stopped."));
        }
    }
}

void AssistApplet::setDaemonStatus(const QString &status)
{
    if (mDaemonStatus == status) {
        return;
    }
    mDaemonStatus = status;
    Q_EMIT daemonStatusChanged();
}

void AssistApplet::fetchStatus()
{
    onReply<QString>(this, mInterface->Status(), [this](const QDBusPendingReply<QString> &reply) {
        if (reply.isError()) {
            qCWarning(PLASMA_ASSIST_APPLET_LOG) << "Impossible to get daemon status:" << reply.error().message();
        } else {
            setDaemonStatus(reply.value());
        }
    });
}

void AssistApplet::setConversationOpen(bool open)
{
    if (mConversationOpen != open) {
        mConversationOpen = open;
        Q_EMIT conversationOpenChanged();
    }
}

void AssistApplet::setAnswerInProgress(bool inProgress)
{
    if (mAnswerInProgress != inProgress) {
        mAnswerInProgress = inProgress;
        Q_EMIT answerInProgressChanged();
    }
}

void AssistApplet::showError(const QString &error)
{
    mConversationModel->addError(error);
    setAnswerInProgress(false);
}

K_PLUGIN_CLASS_WITH_JSON(AssistApplet, "metadata.json")

#include "assistapplet.moc"
