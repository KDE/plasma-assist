/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <chrono>
#include <functional>

class QDBusServiceWatcher;
class QTimer;

// Asks user (with D-Bus client) to confirm tools which change system.
// Rules: actions are disabled by default, no answer or no client means Deny, only client which sent message can answer,
// "always allow" is never used for destructive tools, summary is built from arguments (not by language model).
// It also asks user to keep a change (display settings) which is reverted by server without answer.
class ConfirmationManager : public QObject
{
    Q_OBJECT
public:
    enum class Answer : uint8_t {
        Deny = 0,
        Allow = 1,
        AlwaysAllow = 2,
    };
    enum class Risk : uint8_t {
        Modify,
        Destructive,
        // Change was applied, server reverts it unless user keeps it: title is question of server
        Keep,
    };
    struct Request {
        QString tool;
        QString title;
        QJsonObject arguments;
        Risk risk = Risk::Destructive;
        QByteArray conversation;
        // D-Bus unique name of client which sent message (empty: called in process)
        QString client;
        // Zero: default timeout
        std::chrono::milliseconds timeout{0};
    };
    using AnswerCallback = std::function<void(bool allowed)>;

    explicit ConfirmationManager(QObject *parent = nullptr);
    ~ConfirmationManager() override;

    // Saved in plasma-assistrc ([General] AllowActions), false by default
    [[nodiscard]] bool actionsAllowed() const;
    void setActionsAllowed(bool allowed);

    void setTimeout(std::chrono::milliseconds timeout);

    // \a callback is called once with answer of user
    void request(const Request &request, const AnswerCallback &callback);
    // Returns false when request is unknown or \a caller is not client of request
    bool confirm(const QString &requestIdentifier, Answer answer, const QString &caller);
    [[nodiscard]] bool hasPendingRequest() const;

    [[nodiscard]] bool isAlwaysAllowed(const QString &tool) const;
    [[nodiscard]] QStringList alwaysAllowedTools() const;
    void setAlwaysAllowedTools(const QStringList &tools);

    // Text shown to user, e.g. "Mute Speakers"
    [[nodiscard]] static QString summary(const QString &tool, const QString &title, const QJsonObject &arguments);
    [[nodiscard]] static QString riskName(Risk risk);

Q_SIGNALS:
    void confirmationRequested(const QString &requestIdentifier,
                               const QByteArray &conversation,
                               const QString &tool,
                               const QString &summary,
                               const QString &argumentsJson,
                               const QString &risk);
    void confirmationFinished(const QString &requestIdentifier, bool allowed);
    // Tool was refused because actions are disabled
    void actionRefused(const QByteArray &conversation, const QString &title);
    void actionsAllowedChanged();

private:
    struct PendingRequest {
        Request request;
        AnswerCallback callback;
        QTimer *timer = nullptr;
    };
    void finish(const QString &requestIdentifier, bool allowed);
    void saveAlwaysAllowedTools() const;

    QHash<QString, PendingRequest> mPendingRequests;
    QStringList mAlwaysAllowedTools;
    QDBusServiceWatcher *const mClientWatcher;
    std::chrono::milliseconds mTimeout;
    bool mActionsAllowed = false;
};
