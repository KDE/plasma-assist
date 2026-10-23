/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "assistant.h"
#include "assistantadaptor.h"
#include "config-plasma-assist.h"
#include "plasmaassist_daemon_debug.h"
#include <QDBusConnection>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

using namespace Qt::Literals::StringLiterals;

Assistant::Assistant(QObject *parent)
    : QObject{parent}
{
    new AssistantAdaptor(this);
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

QString Assistant::Status() const
{
    // TODO: report language model and MCP server status
    const QJsonObject status{
        {u"version"_s, QStringLiteral(PLASMA_ASSIST_VERSION)},
        {u"ready"_s, false},
        {u"conversations"_s, mConversations.count()},
    };
    return QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact));
}

QString Assistant::NewConversation()
{
    const QString identifier = QUuid::createUuid().toString(QUuid::WithoutBraces);
    mConversations.append(identifier);
    Q_EMIT StatusChanged(Status());
    return identifier;
}

#include "moc_assistant.cpp"
