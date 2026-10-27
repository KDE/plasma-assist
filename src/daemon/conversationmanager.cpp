/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "conversationmanager.h"
#include "confirmationmanager.h"
#include "plasmaassist_daemon_debug.h"
#include "troubleshooterclient.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <TextAutoGenerateText/TextAutoGenerateChatsModel>
#include <TextAutoGenerateText/TextAutoGenerateLocalDatabaseManager>
#include <TextAutoGenerateText/TextAutoGenerateManager>
#include <TextAutoGenerateText/TextAutoGenerateMcpToolsManager>
#include <TextAutoGenerateText/TextAutoGenerateMessagesModel>
#include <TextAutoGenerateText/TextAutoGenerateTextInstancesManagerDialog>
#include <TextAutoGenerateText/TextAutoGenerateTextPlugin>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolElicitRequestFormParams>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolElicitRequestURLParams>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolElicitResult>
#include <TextAutoGenerateTextMcpProtocolCore/McpServerManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpServerModel>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace TextAutoGenerateText;

namespace
{
constexpr int maximumTitleLength = 80;
// Server reverts a change after its own timeout: keep question can't wait longer
constexpr qint64 defaultKeepSeconds = 15;
constexpr qint64 maximumKeepSeconds = 120;

TextAutoGenerateTextMcpProtocolCore::McpProtocolElicitResult elicitResult(bool accepted)
{
    TextAutoGenerateTextMcpProtocolCore::McpProtocolElicitResult result;
    result.setAction(accepted ? TextAutoGenerateTextMcpProtocolCore::McpProtocolElicitResult::Action::Accept
                              : TextAutoGenerateTextMcpProtocolCore::McpProtocolElicitResult::Action::Decline);
    return result;
}

QString systemPrompt()
{
    return u"You are Plasma Assist, a troubleshooting assistant for the KDE Plasma desktop on Linux. "
           "Use the available tools to diagnose the problem before answering: don't guess the state of the system. "
           "Hints returned by tools point to likely causes. "
           "Values like <ip-1>, <mac-1>, <name-1>, <user> or <hostname> are personal data masked on purpose: use them as they are. "
           "Answer in the language of the user, be concise, and give concrete steps (System Settings, applets). "
           "If a tool is refused, explain how users can make the change themselves."_s;
}

QJsonArray toolCalls(const QJsonArray &toolExchange, const std::function<QString(const QString &)> &toolName)
{
    // Tool exchange contains assistant entries (tool_calls) followed by their results (role "tool")
    QHash<QString, QString> results;
    for (const QJsonValue &entry : toolExchange) {
        const QJsonObject obj = entry.toObject();
        if (obj.value("role"_L1).toString() == "tool"_L1) {
            results.insert(obj.value("tool_call_id"_L1).toString(), obj.value("content"_L1).toString());
        }
    }
    QJsonArray calls;
    for (const QJsonValue &entry : toolExchange) {
        const QJsonArray entryCalls = entry.toObject().value("tool_calls"_L1).toArray();
        for (const QJsonValue &call : entryCalls) {
            QJsonObject obj = call.toObject();
            obj.insert("name"_L1, toolName(obj.value("name"_L1).toString()));
            obj.insert("result"_L1, results.value(obj.value("id"_L1).toString()));
            calls.append(obj);
        }
    }
    return calls;
}
}

ConversationManager::ConversationManager(QObject *parent)
    : QObject{parent}
{
    mEngineTimer.setSingleShot(true);
    mEngineTimer.setInterval(30s);
    connect(&mEngineTimer, &QTimer::timeout, this, [this]() {
        setEngineStatus(EngineStatus::Error, i18n("Language model does not answer."));
    });
}

ConversationManager::~ConversationManager() = default;

bool ConversationManager::remoteLanguageModelAllowed()
{
    const KConfigGroup group(KSharedConfig::openConfig(u"plasma-assistrc"_s), u"General"_s);
    return group.readEntry("AllowRemoteLanguageModel", false);
}

ConversationManager::EngineStatus ConversationManager::engineStatus() const
{
    return mEngineStatus;
}

QString ConversationManager::engineStatusName() const
{
    return QString::fromLatin1(QMetaEnum::fromType<EngineStatus>().valueToKey(static_cast<int>(mEngineStatus))).toLower();
}

QString ConversationManager::errorString() const
{
    return mErrorString;
}

QString ConversationManager::engineName() const
{
    if (!mManager || !mManager->textAutoGeneratePlugin()) {
        return {};
    }
    return mManager->textAutoGeneratePlugin()->displayName();
}

QString ConversationManager::modelName() const
{
    if (!mManager || !mManager->textAutoGeneratePlugin()) {
        return {};
    }
    return mManager->textAutoGeneratePlugin()->currentModel();
}

void ConversationManager::ensureManager()
{
    if (mManager) {
        return;
    }
    mManager = new TextAutoGenerateManager(this);
    mManager->setSystemPrompt(systemPrompt());
    // Don't use loadHistory(): it creates an empty chat
    mManager->textAutoGenerateChatsModel()->setChats(mManager->databaseManager()->loadChats());

    // Server is only known by daemon: it's not saved in configuration of MCP servers
    mManager->textAutoGenerateTextMcpServerManager()->mcpServerModel()->addMcpServer(TroubleshooterClient::server());
    // Handler is always defined: without it, tools are called without confirmation
    mManager->textAutoGenerateMcpToolsManager()->setConfirmationHandler(
        [this](const TextAutoGenerateMcpToolsManager::ToolConfirmationInfo &info,
               const std::function<void(TextAutoGenerateMcpToolsManager::ToolConfirmation)> &answer) {
            if (!mConfirmationManager) {
                qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Refuse tool which modifies system:" << info.tool.name;
                answer(TextAutoGenerateMcpToolsManager::ToolConfirmation::Deny);
                return;
            }
            // Handler doesn't know conversation: it's the one which waits for an answer
            const QByteArray conversation = mConversationsInProgress.isEmpty() ? QByteArray() : *mConversationsInProgress.cbegin();
            const ConfirmationManager::Request request{
                .tool = info.tool.name,
                .title = info.tool.displayName.isEmpty() ? info.tool.name : info.tool.displayName,
                .arguments = info.arguments,
                .risk = info.tool.risk == TextAutoGenerateMcpToolsManager::ToolRisk::Modify ? ConfirmationManager::Risk::Modify
                                                                                            : ConfirmationManager::Risk::Destructive,
                .conversation = conversation,
                .client = mConversationClients.value(conversation),
            };
            // "Always allow" of ktextaddons would allow destructive tools too: it's managed by confirmation manager
            mConfirmationManager->request(request, [answer](bool allowed) {
                answer(allowed ? TextAutoGenerateMcpToolsManager::ToolConfirmation::Allow : TextAutoGenerateMcpToolsManager::ToolConfirmation::Deny);
            });
        });

    // Troubleshooter asks to keep a change (display settings) with a question without field
    mManager->textAutoGenerateMcpToolsManager()->setElicitationHandler(
        [this](const TextAutoGenerateMcpToolsManager::ElicitationInfo &info,
               const std::function<void(const TextAutoGenerateTextMcpProtocolCore::McpProtocolElicitResult &)> &answer) {
            const TextAutoGenerateTextMcpProtocolCore::McpProtocolUtils::ElicitRequestParams params = info.request.params();
            const auto *form = std::get_if<TextAutoGenerateTextMcpProtocolCore::McpProtocolElicitRequestFormParams>(&params);
            if (!mConfirmationManager || info.serverName != TroubleshooterClient::server().name() || !form || !form->requestedSchema().properties().isEmpty()) {
                qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Unsupported elicitation from" << info.serverName;
                answer(elicitResult(false));
                return;
            }
            qint64 seconds = defaultKeepSeconds;
            if (const auto meta = form->meta(); meta) {
                seconds = meta->additionalProperties().value("org.kde/timeoutSeconds"_L1).toInteger(defaultKeepSeconds);
            }
            seconds = std::clamp<qint64>(seconds, 1, maximumKeepSeconds);
            const QByteArray conversation = mConversationsInProgress.isEmpty() ? QByteArray() : *mConversationsInProgress.cbegin();
            const ConfirmationManager::Request request{
                .tool = {},
                .title = form->message(),
                .arguments = {{u"timeoutSeconds"_s, seconds}},
                .risk = ConfirmationManager::Risk::Keep,
                .conversation = conversation,
                .client = mConversationClients.value(conversation),
                .timeout = std::chrono::seconds(seconds),
            };
            mConfirmationManager->request(request, [answer](bool keep) {
                answer(elicitResult(keep));
            });
        });

    // Another language model can be selected: it's checked again (remote models are refused) before next message
    connect(mManager, &TextAutoGenerateManager::configChanged, this, [this]() {
        if (mEngineStatus != EngineStatus::Loading) {
            setEngineStatus(EngineStatus::NotLoaded);
        }
    });
    connect(mManager, &TextAutoGenerateManager::chatInProgressStatusChanged, this, &ConversationManager::slotConversationInProgressChanged);
    connect(mManager, &TextAutoGenerateManager::errorOccurred, this, [this](QNetworkReply::NetworkError, const QString &error) {
        // Error is not associated to a conversation
        for (const QByteArray &conversation : std::as_const(mConversationsInProgress)) {
            Q_EMIT errorOccurred(conversation, error);
        }
    });
}

void ConversationManager::loadEngine()
{
    setEngineStatus(EngineStatus::Loading);
    connect(mManager, &TextAutoGenerateManager::pluginsInitializedDone, this, &ConversationManager::slotEngineLoaded, Qt::SingleShotConnection);
    mEngineTimer.start();
    mManager->loadEngine();
    // Don't wait for initialization of a language model which can't be used
    if (const QString error = engineError(); mEngineStatus == EngineStatus::Loading && !error.isEmpty()) {
        mEngineTimer.stop();
        disconnect(mManager, &TextAutoGenerateManager::pluginsInitializedDone, this, &ConversationManager::slotEngineLoaded);
        setEngineStatus(EngineStatus::Error, error);
    }
}

QString ConversationManager::engineError() const
{
    const TextAutoGenerateTextPlugin *plugin = mManager->textAutoGeneratePlugin();
    if (!plugin) {
        return i18n("No language model is configured.");
    }
    if (!plugin->isLocalServer() && !remoteLanguageModelAllowed()) {
        // Information about system must not leave the computer without agreement of user
        return i18n("Language model \"%1\" sends data to a remote server, it's not allowed. Select a local language model.", plugin->displayName());
    }
    return {};
}

QString ConversationManager::modelError() const
{
    const TextAutoGenerateTextPlugin *plugin = mManager->textAutoGeneratePlugin();
    // Without selected model, ktextaddons uses first model which can answer messages
    if (plugin && plugin->currentModel().isEmpty()) {
        return i18n("No model is selected for language model \"%1\". Select one in its configuration.", plugin->displayName());
    }
    return {};
}

void ConversationManager::slotEngineLoaded()
{
    mEngineTimer.stop();
    if (const QString error = engineError(); !error.isEmpty()) {
        setEngineStatus(EngineStatus::Error, error);
    } else if (const QString noModelError = modelError(); !noModelError.isEmpty() && mManager->textAutoGeneratePlugin()->ready()) {
        setEngineStatus(EngineStatus::Error, noModelError);
    } else if (!mManager->textAutoGeneratePlugin()->ready()) {
        setEngineStatus(EngineStatus::Error, i18n("Language model \"%1\" is not ready.", mManager->textAutoGeneratePlugin()->displayName()));
    } else {
        setEngineStatus(EngineStatus::Ready);
    }
}

void ConversationManager::setEngineStatus(EngineStatus status, const QString &errorString)
{
    if (status == EngineStatus::Error) {
        qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Language model error:" << errorString;
    }
    const bool changed = mEngineStatus != status || mErrorString != errorString;
    mEngineStatus = status;
    mErrorString = errorString;
    if (status == EngineStatus::Ready || status == EngineStatus::Error) {
        const QList<PendingMessage> messages = std::exchange(mPendingMessages, {});
        for (const PendingMessage &message : messages) {
            if (status == EngineStatus::Ready) {
                send(message);
            } else {
                mConversationsInProgress.remove(message.conversation);
                Q_EMIT errorOccurred(message.conversation, errorString);
            }
        }
    }
    if (changed) {
        Q_EMIT engineStatusChanged();
    }
}

void ConversationManager::showConfigureDialog()
{
    ensureManager();
    if (!mConfigureDialog) {
        mConfigureDialog = new TextAutoGenerateTextInstancesManagerDialog(mManager);
        mConfigureDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(mConfigureDialog, &QDialog::accepted, this, [this]() {
            // Instances were saved: they are loaded again (and checked) for next message
            if (mEngineStatus != EngineStatus::Loading) {
                setEngineStatus(EngineStatus::NotLoaded);
            }
        });
    }
    mConfigureDialog->show();
    mConfigureDialog->raise();
    mConfigureDialog->activateWindow();
}

bool ConversationManager::isConfigureDialogVisible() const
{
    return mConfigureDialog && mConfigureDialog->isVisible();
}

QByteArray ConversationManager::newConversation()
{
    ensureManager();
    const QList<QByteArray> previousConversations = mManager->textAutoGenerateChatsModel()->chatsId();
    // New chat becomes current chat only when language model is initialized: look for it in chats
    mManager->createNewChat({}, TextAutoGenerateChat::Persistence::Persisted);
    const QList<QByteArray> conversations = mManager->textAutoGenerateChatsModel()->chatsId();
    const auto it = std::ranges::find_if(conversations, [&previousConversations](const QByteArray &conversation) noexcept {
        return !previousConversations.contains(conversation);
    });
    if (it == conversations.cend()) {
        qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Impossible to create conversation";
        return {};
    }
    Q_EMIT conversationsChanged();
    return *it;
}

QList<ConversationManager::Conversation> ConversationManager::conversations()
{
    ensureManager();
    const QList<TextAutoGenerateChat> chats = mManager->textAutoGenerateChatsModel()->chats();
    QList<Conversation> result;
    result.reserve(chats.count());
    for (const TextAutoGenerateChat &chat : chats) {
        result.append({.identifier = chat.identifier(), .title = chat.title(), .dateTime = chat.dateTime()});
    }
    return result;
}

bool ConversationManager::hasConversation(const QByteArray &conversation)
{
    ensureManager();
    return mManager->textAutoGenerateChatsModel()->chatsId().contains(conversation);
}

void ConversationManager::clearConversations()
{
    ensureManager();
    const QList<QByteArray> conversations = mManager->textAutoGenerateChatsModel()->chatsId();
    bool removed = false;
    for (const QByteArray &conversation : conversations) {
        if (mConversationsInProgress.contains(conversation)) {
            continue;
        }
        mManager->removeDiscussion(conversation);
        mWatchedConversations.remove(conversation);
        mConversationClients.remove(conversation);
        removed = true;
    }
    if (removed) {
        Q_EMIT conversationsChanged();
    }
}

TextAutoGenerateMessagesModel *ConversationManager::messagesModel(const QByteArray &conversation)
{
    if (!hasConversation(conversation)) {
        return nullptr;
    }
    // Messages of conversation are loaded from database when it becomes current chat
    mManager->setCurrentChatId(conversation);
    watchConversation(conversation);
    return mManager->messagesModelFromChatId(conversation);
}

QJsonArray ConversationManager::messages(const QByteArray &conversation)
{
    const TextAutoGenerateMessagesModel *model = messagesModel(conversation);
    if (!model) {
        return {};
    }
    QJsonArray result;
    const QList<TextAutoGenerateMessage> messages = model->messages();
    for (const TextAutoGenerateMessage &message : messages) {
        result.append(QJsonObject{
            {u"id"_s, QString::fromLatin1(message.uuid())},
            {u"role"_s, message.sender() == TextAutoGenerateMessage::Sender::User ? u"user"_s : u"assistant"_s},
            {u"text"_s, message.content()},
            {u"dateTime"_s, QDateTime::fromSecsSinceEpoch(message.dateTime()).toString(Qt::ISODate)},
            {u"inProgress"_s, message.inProgress()},
            {u"toolCalls"_s,
             toolCalls(message.toolExchange(),
                       [this](const QString &exposedName) {
                           return toolName(exposedName);
                       })},
        });
    }
    return result;
}

void ConversationManager::sendScenario(const QByteArray &conversation, const QString &text, const QString &client)
{
    ensureManager();
    TextAutoGenerateMcpToolsManager *toolsManager = mManager->textAutoGenerateMcpToolsManager();
    // Tools of server must be loaded to know their exposed names ("server__tool")
    toolsManager->prepareServers({TroubleshooterClient::serverIdentifier()}, this, [this, toolsManager, conversation, text, client]() {
        const QString prompt = toolsManager->replaceToolNames(TroubleshooterClient::serverIdentifier(), text);
        if (const QString error = sendMessage(conversation, prompt, client); !error.isEmpty()) {
            Q_EMIT errorOccurred(conversation, error);
        }
    });
}

void ConversationManager::setConfirmationManager(ConfirmationManager *confirmationManager)
{
    mConfirmationManager = confirmationManager;
}

QString ConversationManager::sendMessage(const QByteArray &conversation, const QString &text, const QString &client)
{
    if (text.trimmed().isEmpty()) {
        return i18n("Message is empty.");
    }
    if (!messagesModel(conversation)) {
        return i18n("Conversation \"%1\" does not exist.", QString::fromLatin1(conversation));
    }
    if (mConversationsInProgress.contains(conversation)) {
        return i18n("Conversation is waiting for an answer.");
    }
    mConversationsInProgress.insert(conversation);
    mConversationClients.insert(conversation, client);
    const PendingMessage message{.conversation = conversation, .text = text};
    switch (mEngineStatus) {
    case EngineStatus::Ready:
        send(message);
        break;
    case EngineStatus::Loading:
        mPendingMessages.append(message);
        break;
    case EngineStatus::NotLoaded:
    case EngineStatus::Error:
        // Configuration can have changed since last error: try again
        mPendingMessages.append(message);
        loadEngine();
        break;
    }
    return {};
}

void ConversationManager::send(const PendingMessage &message)
{
    // Current language model can be changed in configuration dialog without loading engine again
    QString error = engineError();
    if (error.isEmpty()) {
        error = modelError();
    }
    if (!error.isEmpty()) {
        mConversationsInProgress.remove(message.conversation);
        setEngineStatus(EngineStatus::Error, error);
        Q_EMIT errorOccurred(message.conversation, error);
        return;
    }
    TextAutoGenerateTextPlugin *plugin = mManager->textAutoGeneratePlugin();
    setTitleIfEmpty(message.conversation, message.text);
    plugin->sendMessage(TextAutoGenerateTextPlugin::EditSendInfo{
        .message = message.text,
        .messageUuid = {},
        .chatId = message.conversation,
        .tools = {TextAutoGenerateMcpToolsManager::toolIdentifier(TroubleshooterClient::serverIdentifier())},
        .attachmentInfoList = {},
    });
}

bool ConversationManager::cancel(const QByteArray &conversation)
{
    if (!mConversationsInProgress.contains(conversation)) {
        return false;
    }
    const auto pending = std::ranges::find(mPendingMessages, conversation, &PendingMessage::conversation);
    if (pending != mPendingMessages.end()) {
        mPendingMessages.erase(pending);
        mConversationsInProgress.remove(conversation);
        return true;
    }
    TextAutoGenerateMessagesModel *model = messagesModel(conversation);
    if (!model) {
        return false;
    }
    return mManager->cancelRequest(conversation, model->indexForUuid(model->lastMessageUuid()));
}

bool ConversationManager::hasConversationInProgress() const
{
    return !mConversationsInProgress.isEmpty();
}

void ConversationManager::setTitleIfEmpty(const QByteArray &conversation, const QString &text)
{
    TextAutoGenerateChatsModel *chatsModel = mManager->textAutoGenerateChatsModel();
    for (int row = 0; row < chatsModel->rowCount(); ++row) {
        const QModelIndex index = chatsModel->index(row);
        if (index.data(TextAutoGenerateChatsModel::Identifier).toByteArray() == conversation) {
            if (chatsModel->title(conversation).isEmpty()) {
                // Title is saved in database by manager
                (void)chatsModel->setData(index, text.simplified().left(maximumTitleLength), TextAutoGenerateChatsModel::Title);
                Q_EMIT conversationsChanged();
            }
            return;
        }
    }
}

void ConversationManager::watchConversation(const QByteArray &conversation)
{
    if (mWatchedConversations.contains(conversation)) {
        return;
    }
    mWatchedConversations.insert(conversation);
    const TextAutoGenerateMessagesModel *model = mManager->messagesModelFromChatId(conversation);
    // Messages already loaded must not be sent again
    const QList<TextAutoGenerateMessage> messages = model->messages();
    for (const TextAutoGenerateMessage &message : messages) {
        mSentAnswers.insert(message.uuid(), message.content());
        mSentToolEntries.insert(message.uuid(), message.toolExchange().count());
    }
    connect(model, &QAbstractItemModel::dataChanged, this, [this, conversation, model](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
            slotMessageChanged(conversation, model->index(row).data(TextAutoGenerateMessagesModel::UuidRole).toByteArray());
        }
    });
    connect(model, &QAbstractItemModel::rowsInserted, this, [this, conversation, model](const QModelIndex &, int first, int last) {
        for (int row = first; row <= last; ++row) {
            slotMessageChanged(conversation, model->index(row).data(TextAutoGenerateMessagesModel::UuidRole).toByteArray());
        }
    });
}

void ConversationManager::slotMessageChanged(const QByteArray &conversation, const QByteArray &messageId)
{
    const TextAutoGenerateMessage message = mManager->messagesModelFromChatId(conversation)->message(messageId);
    if (message.sender() != TextAutoGenerateMessage::Sender::Assistant) {
        return;
    }
    // ktextaddons adds tool calls and their results together: both signals are emitted when results are known
    const QJsonArray toolExchange = message.toolExchange();
    qsizetype &sentEntries = mSentToolEntries[messageId];
    for (; sentEntries < toolExchange.count(); ++sentEntries) {
        const QJsonObject entry = toolExchange.at(sentEntries).toObject();
        if (entry.value("role"_L1).toString() == "tool"_L1) {
            // Result is the text given to language model: MCP error flag is not available
            Q_EMIT toolCallFinished(conversation, entry.value("tool_call_id"_L1).toString(), entry.value("content"_L1).toString(), false);
            continue;
        }
        const QJsonArray calls = entry.value("tool_calls"_L1).toArray();
        for (const QJsonValue &value : calls) {
            const QJsonObject call = value.toObject();
            Q_EMIT toolCallStarted(conversation,
                                   call.value("id"_L1).toString(),
                                   toolName(call.value("name"_L1).toString()),
                                   QString::fromUtf8(QJsonDocument(call.value("arguments"_L1).toObject()).toJson(QJsonDocument::Compact)));
        }
    }
    if (const QString answer = message.content(); !answer.isEmpty() && mSentAnswers.value(messageId) != answer) {
        mSentAnswers.insert(messageId, answer);
        Q_EMIT messageChunk(conversation, messageId, answer);
    }
}

void ConversationManager::slotConversationInProgressChanged(const QByteArray &conversation, bool inProgress)
{
    if (inProgress || !mConversationsInProgress.remove(conversation)) {
        return;
    }
    const TextAutoGenerateMessagesModel *model = mManager->messagesModelFromChatId(conversation);
    const QByteArray messageId = model ? model->lastMessageUuid() : QByteArray();
    // Last chunk can be received after progress status
    slotMessageChanged(conversation, messageId);
    Q_EMIT messageFinished(conversation, messageId);
}

QString ConversationManager::toolName(const QString &exposedName) const
{
    if (const auto tool = mManager->textAutoGenerateMcpToolsManager()->tool(exposedName.toLatin1()); tool) {
        return tool->name;
    }
    // Tools are unknown until server is connected: drop server prefix ("server__tool")
    if (const qsizetype separator = exposedName.indexOf("__"_L1); separator >= 0) {
        return exposedName.mid(separator + 2);
    }
    return exposedName;
}

#include "moc_conversationmanager.cpp"
