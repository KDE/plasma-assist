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
        // "user", "assistant", "tool" or "error"
        TypeRole = Qt::UserRole + 1,
        // Markdown for assistant
        TextRole,
        // Tool calls: name, "running", "done" or "failed", raw JSON
        ToolNameRole,
        StatusRole,
        ArgumentsRole,
        ResultRole,
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

private:
    struct Item {
        QString type;
        QString identifier;
        QString text;
        QString toolName;
        QString status;
        QString arguments;
        QString result;
    };
    void append(const Item &item);
    [[nodiscard]] qsizetype indexOf(const QString &type, const QString &identifier) const;

    QList<Item> mItems;
};
