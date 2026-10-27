/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QAbstractListModel>

// Items of conversation shown by applet: messages, tool calls and errors.
// It's filled with events of daemon (D-Bus signals).
class ConversationModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        // "user", "assistant", "tool", "confirmation", "notice" or "error"
        TypeRole = Qt::UserRole + 1,
        // Markdown for assistant
        TextRole,
        // Tool calls: name, "running", "done" or "failed", raw JSON
        // Confirmations: "pending", "allowed" or "denied"
        ToolNameRole,
        StatusRole,
        ArgumentsRole,
        ResultRole,
        // Identifier of confirmation request
        IdentifierRole,
        // Confirmations: "modify" or "destructive"
        RiskRole,
    };
    Q_ENUM(Roles)

    explicit ConversationModel(QObject *parent = nullptr);
    ~ConversationModel() override;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void clear();
    // JSON array returned by GetMessages()
    void setMessages(const QString &json);
    void addUserMessage(const QString &text);
    // \a text is the whole answer received so far
    void setAnswer(const QString &messageId, const QString &text);
    void startToolCall(const QString &callId, const QString &toolName, const QString &arguments);
    void finishToolCall(const QString &callId, const QString &result, bool isError);
    void addError(const QString &text);
    void addNotice(const QString &text);
    // \a summary is shown as text
    void addConfirmation(const QString &requestId, const QString &toolName, const QString &summary, const QString &arguments, const QString &risk);
    void finishConfirmation(const QString &requestId, bool allowed);
    [[nodiscard]] bool hasPendingConfirmation() const;

private:
    struct Item {
        QString type;
        QString identifier;
        QString text;
        QString toolName;
        QString status;
        QString arguments;
        QString result;
        QString risk;
    };
    void append(const Item &item);
    [[nodiscard]] qsizetype indexOf(const QString &type, const QString &identifier) const;

    QList<Item> mItems;
};
