/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "troubleshooterclient.h"
#include "plasmaassist_daemon_debug.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaEnum>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolSettings>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
constexpr QLatin1StringView serverBinaryName{"plasma-assist-troubleshooter"};

QJsonObject errorAnswer(const QString &message)
{
    return QJsonObject{{u"error"_s, QJsonObject{{u"code"_s, -32000}, {u"message"_s, message}}}};
}
}

TroubleshooterClient::TroubleshooterClient(QObject *parent)
    : QObject{parent}
{
}

TroubleshooterClient::~TroubleshooterClient() = default;

QString TroubleshooterClient::serverPath()
{
    if (const QString path = qEnvironmentVariable("PLASMA_ASSIST_TROUBLESHOOTER"); !path.isEmpty()) {
        return path;
    }
    // Not installed (build directory): use server built with daemon
    if (const QFileInfo sibling(QDir(QCoreApplication::applicationDirPath()).filePath(serverBinaryName)); sibling.isExecutable()) {
        return sibling.absoluteFilePath();
    }
    return QStringLiteral(PLASMA_ASSIST_TROUBLESHOOTER_BINARY);
}

QByteArray TroubleshooterClient::serverIdentifier()
{
    return "plasma-assist-troubleshooter"_ba;
}

McpServer TroubleshooterClient::server()
{
    McpProtocolSettings settings;
    settings.setCommand(serverPath());
    McpServer server;
    server.setIdentifier(serverIdentifier());
    server.setName(u"Plasma Assist Troubleshooter"_s);
    server.setTransportType(McpProtocolPlugin::TransportType::Stdio);
    server.setSettings(settings);
    server.setEnabled(true);
    return server;
}

TroubleshooterClient::Status TroubleshooterClient::status() const
{
    return mStatus;
}

QString TroubleshooterClient::statusName() const
{
    return QString::fromLatin1(QMetaEnum::fromType<Status>().valueToKey(static_cast<int>(mStatus))).toLower();
}

QString TroubleshooterClient::errorString() const
{
    return mErrorString;
}

QJsonArray TroubleshooterClient::tools() const
{
    return mTools;
}

QJsonArray TroubleshooterClient::prompts() const
{
    return mPrompts;
}

bool TroubleshooterClient::hasPendingRequest() const
{
    return !mPendingActions.isEmpty() || !mPendingCalls.isEmpty();
}

void TroubleshooterClient::whenReady(QObject *context, const std::function<void(const QString &error)> &callback)
{
    if (mStatus == Status::Ready) {
        callback({});
        return;
    }
    mPendingActions.append(PendingAction{.context = context, .callback = callback});
    if (mStatus != Status::Starting) {
        start();
    }
}

void TroubleshooterClient::callTool(const QString &name, const QJsonObject &arguments, QObject *context, const AnswerCallback &callback)
{
    whenReady(context, [this, name, arguments, context, callback](const QString &error) {
        if (!error.isEmpty()) {
            callback(errorAnswer(error));
            return;
        }
        const qint64 requestId = mClient->callTool(name, arguments);
        if (requestId < 0) {
            callback(errorAnswer(u"Impossible to call tool %1"_s.arg(name)));
            return;
        }
        mPendingCalls.insert(requestId, {context, callback});
    });
}

void TroubleshooterClient::getPrompt(const QString &name, const QMap<QString, QString> &arguments, QObject *context, const AnswerCallback &callback)
{
    whenReady(context, [this, name, arguments, context, callback](const QString &error) {
        if (!error.isEmpty()) {
            callback(errorAnswer(error));
            return;
        }
        const qint64 requestId = mClient->getPrompt(name, arguments);
        if (requestId < 0) {
            callback(errorAnswer(u"Impossible to get prompt %1"_s.arg(name)));
            return;
        }
        mPendingCalls.insert(requestId, {context, callback});
    });
}

void TroubleshooterClient::stop()
{
    if (mClient) {
        mClient->stopClient();
    }
}

void TroubleshooterClient::start()
{
    if (!mClient) {
        mClient = new McpProtocolClientProtocolManager(server(), this);
        mClient->setClientName(u"plasma-assistd"_s);
        connect(mClient, &McpProtocolClientProtocolManager::initialized, this, [this]() {
            // Server is ready when tools and prompts are known
            mListToolsRequestId = mClient->executeAction(McpProtocolClientProtocolManager::MethodType::ListTools);
            mListPromptsRequestId = mClient->executeAction(McpProtocolClientProtocolManager::MethodType::ListPrompts);
        });
        connect(mClient, &McpProtocolClientProtocolManager::received, this, &TroubleshooterClient::slotReceived);
        connect(mClient, &McpProtocolClientProtocolManager::error, this, [this](const QString &error) {
            qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Troubleshooter error:" << error;
            setStatus(Status::Error, error);
            failPendingRequests(error);
        });
        connect(mClient, &McpProtocolClientProtocolManager::finished, this, [this]() {
            if (mStatus != Status::Error) {
                setStatus(Status::Stopped);
            }
            failPendingRequests(u"Troubleshooter server was stopped"_s);
        });
    }
    setStatus(Status::Starting);
    mClient->initializeClient();
}

void TroubleshooterClient::slotReceived(const QJsonObject &obj, McpProtocolClientProtocolManager::MethodType type)
{
    const qint64 requestId = obj.value("id"_L1).toInteger(-1);
    if (type == McpProtocolClientProtocolManager::MethodType::ListTools && requestId == mListToolsRequestId) {
        mListToolsRequestId = -1;
        if (obj.contains("error"_L1)) {
            const QString error = obj.value("error"_L1).toObject().value("message"_L1).toString();
            setStatus(Status::Error, error);
            failPendingRequests(error);
            return;
        }
        mTools = obj.value("result"_L1).toObject().value("tools"_L1).toArray();
        setReadyWhenListsAreReceived();
        return;
    }
    if (type == McpProtocolClientProtocolManager::MethodType::ListPrompts && requestId == mListPromptsRequestId) {
        mListPromptsRequestId = -1;
        // Scenarios are optional
        mPrompts = obj.value("result"_L1).toObject().value("prompts"_L1).toArray();
        setReadyWhenListsAreReceived();
        return;
    }
    if (type == McpProtocolClientProtocolManager::MethodType::CallTool || type == McpProtocolClientProtocolManager::MethodType::GetPrompt) {
        const auto pending = mPendingCalls.take(requestId);
        if (pending.first) {
            pending.second(obj);
        }
    }
}

void TroubleshooterClient::setReadyWhenListsAreReceived()
{
    if (mListToolsRequestId >= 0 || mListPromptsRequestId >= 0 || mStatus != Status::Starting) {
        return;
    }
    setStatus(Status::Ready);
    const QList<PendingAction> actions = std::exchange(mPendingActions, {});
    for (const PendingAction &action : actions) {
        if (action.context) {
            action.callback({});
        }
    }
}

void TroubleshooterClient::failPendingRequests(const QString &error)
{
    const QList<PendingAction> actions = std::exchange(mPendingActions, {});
    for (const PendingAction &action : actions) {
        if (action.context) {
            action.callback(error);
        }
    }
    const auto calls = std::exchange(mPendingCalls, {});
    for (const auto &[context, callback] : calls) {
        if (context) {
            callback(errorAnswer(error));
        }
    }
}

void TroubleshooterClient::setStatus(Status status, const QString &errorString)
{
    if (mStatus == status && mErrorString == errorString) {
        return;
    }
    mStatus = status;
    mErrorString = errorString;
    Q_EMIT statusChanged();
}

#include "moc_troubleshooterclient.cpp"
