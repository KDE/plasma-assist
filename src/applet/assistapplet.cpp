/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "assistapplet.h"
#include "assistantinterface.h"
#include "plasmaassist_applet_debug.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

using namespace Qt::Literals::StringLiterals;
namespace
{
constexpr QLatin1StringView serviceName{"org.kde.plasma.assist"};
}

AssistApplet::AssistApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , mInterface(new OrgKdePlasmaAssistAssistantInterface(serviceName, u"/Assistant"_s, QDBusConnection::sessionBus(), this))
    , mServiceWatcher(new QDBusServiceWatcher(serviceName, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this))
{
    connect(mServiceWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        setDaemonAvailable(true);
    });
    connect(mServiceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        setDaemonAvailable(false);
    });
    connect(mInterface, &OrgKdePlasmaAssistAssistantInterface::StatusChanged, this, &AssistApplet::setDaemonStatus);
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

void AssistApplet::startDaemon()
{
    if (mDaemonAvailable) {
        return;
    }
    // Async: serviceRegistered will be emitted when daemon is started
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().interface()->asyncCall(u"StartServiceByName"_s, serviceName, 0U), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<uint> reply = *call;
        if (reply.isError()) {
            qCWarning(PLASMA_ASSIST_APPLET_LOG) << "Impossible to start daemon:" << reply.error().message();
        }
        call->deleteLater();
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
    } else {
        setDaemonStatus({});
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
    auto watcher = new QDBusPendingCallWatcher(mInterface->Status(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        if (reply.isError()) {
            qCWarning(PLASMA_ASSIST_APPLET_LOG) << "Impossible to get daemon status:" << reply.error().message();
        } else {
            setDaemonStatus(reply.value());
        }
        call->deleteLater();
    });
}

K_PLUGIN_CLASS_WITH_JSON(AssistApplet, "metadata.json")

#include "assistapplet.moc"
