/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolClientProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpServer>
#include <functional>

// MCP client of plasma-assist-troubleshooter (stdio): server is started on first request
class TroubleshooterClient : public QObject
{
    Q_OBJECT
public:
    enum class Status : uint8_t {
        Stopped,
        Starting,
        Ready,
        Error,
    };
    Q_ENUM(Status)

    // Raw JSON-RPC answer: contains "result" or "error"
    using AnswerCallback = std::function<void(const QJsonObject &answer)>;

    explicit TroubleshooterClient(QObject *parent = nullptr);
    ~TroubleshooterClient() override;

    // PLASMA_ASSIST_TROUBLESHOOTER environment variable, binary next to daemon (build directory) or installed binary
    [[nodiscard]] static QString serverPath();
    [[nodiscard]] static QByteArray serverIdentifier();
    // Stdio server which starts serverPath()
    [[nodiscard]] static TextAutoGenerateTextMcpProtocolCore::McpServer server();

    [[nodiscard]] Status status() const;
    [[nodiscard]] QString statusName() const;
    [[nodiscard]] QString errorString() const;
    // Tools sent by server (tools/list)
    [[nodiscard]] QJsonArray tools() const;
    // Prompts sent by server (prompts/list): guided scenarios
    [[nodiscard]] QJsonArray prompts() const;

    // \a callback is not called when \a context is deleted
    void whenReady(QObject *context, const std::function<void(const QString &error)> &callback);
    void callTool(const QString &name, const QJsonObject &arguments, QObject *context, const AnswerCallback &callback);
    void getPrompt(const QString &name, const QMap<QString, QString> &arguments, QObject *context, const AnswerCallback &callback);
    [[nodiscard]] bool hasPendingRequest() const;

    void stop();

Q_SIGNALS:
    void statusChanged();

private:
    struct PendingAction {
        QPointer<QObject> context;
        std::function<void(const QString &error)> callback;
    };
    void start();
    void setStatus(Status status, const QString &errorString = {});
    void slotReceived(const QJsonObject &obj, TextAutoGenerateTextMcpProtocolCore::McpProtocolClientProtocolManager::MethodType type);
    void failPendingRequests(const QString &error);
    void setReadyWhenListsAreReceived();

    TextAutoGenerateTextMcpProtocolCore::McpProtocolClientProtocolManager *mClient = nullptr;
    QList<PendingAction> mPendingActions;
    QHash<qint64, std::pair<QPointer<QObject>, AnswerCallback>> mPendingCalls;
    QJsonArray mTools;
    QJsonArray mPrompts;
    qint64 mListPromptsRequestId = -1;
    QString mErrorString;
    qint64 mListToolsRequestId = -1;
    Status mStatus = Status::Stopped;
};
