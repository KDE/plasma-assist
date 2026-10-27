/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "conversationmodel.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::Literals::StringLiterals;

namespace
{
constexpr QLatin1StringView userType{"user"};
constexpr QLatin1StringView assistantType{"assistant"};
constexpr QLatin1StringView toolType{"tool"};
constexpr QLatin1StringView errorType{"error"};
constexpr QLatin1StringView noticeType{"notice"};
constexpr QLatin1StringView confirmationType{"confirmation"};

QString toJson(const QJsonValue &value)
{
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return value.toString();
}
}

ConversationModel::ConversationModel(QObject *parent)
    : QAbstractListModel{parent}
{
}

ConversationModel::~ConversationModel() = default;

int ConversationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return mItems.count();
}

QVariant ConversationModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Item &item = mItems.at(index.row());
    switch (role) {
    case TypeRole:
        return item.type;
    case Qt::DisplayRole:
    case TextRole:
        return item.text;
    case ToolNameRole:
        return item.toolName;
    case StatusRole:
        return item.status;
    case ArgumentsRole:
        return item.arguments;
    case ResultRole:
        return item.result;
    case IdentifierRole:
        return item.identifier;
    case RiskRole:
        return item.risk;
    default:
        return {};
    }
}

QHash<int, QByteArray> ConversationModel::roleNames() const
{
    return {
        {TypeRole, "type"_ba},
        {TextRole, "text"_ba},
        {ToolNameRole, "toolName"_ba},
        {StatusRole, "status"_ba},
        {ArgumentsRole, "arguments"_ba},
        {ResultRole, "result"_ba},
        {IdentifierRole, "identifier"_ba},
        {RiskRole, "risk"_ba},
    };
}

void ConversationModel::clear()
{
    beginResetModel();
    mItems.clear();
    endResetModel();
}

void ConversationModel::setMessages(const QString &json)
{
    QList<Item> items;
    const QJsonArray messages = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue &value : messages) {
        const QJsonObject message = value.toObject();
        // Tools were called before answer
        const QJsonArray toolCalls = message.value("toolCalls"_L1).toArray();
        for (const QJsonValue &toolCall : toolCalls) {
            const QJsonObject call = toolCall.toObject();
            items.append({
                .type = toolType,
                .identifier = call.value("id"_L1).toString(),
                .text = {},
                .toolName = call.value("name"_L1).toString(),
                .status = u"done"_s,
                .arguments = toJson(call.value("arguments"_L1)),
                .result = call.value("result"_L1).toString(),
                .risk = {},
            });
        }
        if (const QString text = message.value("text"_L1).toString(); !text.isEmpty()) {
            items.append({
                .type = message.value("role"_L1).toString() == userType ? userType : assistantType,
                .identifier = message.value("id"_L1).toString(),
                .text = text,
                .toolName = {},
                .status = {},
                .arguments = {},
                .result = {},
                .risk = {},
            });
        }
    }
    beginResetModel();
    mItems = items;
    endResetModel();
}

void ConversationModel::addUserMessage(const QString &text)
{
    append({.type = userType, .identifier = {}, .text = text, .toolName = {}, .status = {}, .arguments = {}, .result = {}, .risk = {}});
}

void ConversationModel::setAnswer(const QString &messageId, const QString &text)
{
    const qsizetype row = indexOf(assistantType, messageId);
    if (row >= 0 && row == mItems.count() - 1) {
        mItems[row].text = text;
        const QModelIndex index = createIndex(row, 0);
        Q_EMIT dataChanged(index, index, {TextRole});
        return;
    }
    if (row >= 0) {
        // Tools were called after beginning of answer: answer is shown after them
        beginRemoveRows({}, row, row);
        mItems.removeAt(row);
        endRemoveRows();
    }
    append({.type = assistantType, .identifier = messageId, .text = text, .toolName = {}, .status = {}, .arguments = {}, .result = {}, .risk = {}});
}

void ConversationModel::startToolCall(const QString &callId, const QString &toolName, const QString &arguments)
{
    append(
        {.type = toolType, .identifier = callId, .text = {}, .toolName = toolName, .status = u"running"_s, .arguments = arguments, .result = {}, .risk = {}});
}

void ConversationModel::finishToolCall(const QString &callId, const QString &result, bool isError)
{
    const qsizetype row = indexOf(toolType, callId);
    if (row < 0) {
        return;
    }
    mItems[row].status = isError ? u"failed"_s : u"done"_s;
    mItems[row].result = result;
    const QModelIndex index = createIndex(row, 0);
    Q_EMIT dataChanged(index, index, {StatusRole, ResultRole});
}

void ConversationModel::addError(const QString &text)
{
    append({.type = errorType, .identifier = {}, .text = text, .toolName = {}, .status = {}, .arguments = {}, .result = {}, .risk = {}});
}

void ConversationModel::addNotice(const QString &text)
{
    append({.type = noticeType, .identifier = {}, .text = text, .toolName = {}, .status = {}, .arguments = {}, .result = {}, .risk = {}});
}

void ConversationModel::addConfirmation(const QString &requestId,
                                        const QString &toolName,
                                        const QString &summary,
                                        const QString &arguments,
                                        const QString &risk)
{
    append({.type = confirmationType,
            .identifier = requestId,
            .text = summary,
            .toolName = toolName,
            .status = u"pending"_s,
            .arguments = arguments,
            .result = {},
            .risk = risk});
}

void ConversationModel::finishConfirmation(const QString &requestId, bool allowed)
{
    const qsizetype row = indexOf(confirmationType, requestId);
    if (row < 0) {
        return;
    }
    mItems[row].status = allowed ? u"allowed"_s : u"denied"_s;
    const QModelIndex index = createIndex(row, 0);
    Q_EMIT dataChanged(index, index, {StatusRole});
}

bool ConversationModel::hasPendingConfirmation() const
{
    return std::ranges::any_of(mItems, [](const Item &item) {
        return item.type == confirmationType && item.status == "pending"_L1;
    });
}

void ConversationModel::append(const Item &item)
{
    beginInsertRows({}, mItems.count(), mItems.count());
    mItems.append(item);
    endInsertRows();
}

qsizetype ConversationModel::indexOf(const QString &type, const QString &identifier) const
{
    for (qsizetype i = mItems.count() - 1; i >= 0; --i) {
        if (mItems.at(i).type == type && mItems.at(i).identifier == identifier) {
            return i;
        }
    }
    return -1;
}

#include "moc_conversationmodel.cpp"
