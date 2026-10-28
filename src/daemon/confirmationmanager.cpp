/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "confirmationmanager.h"
#include "plasmaassist_daemon_debug.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QTimer>
#include <QUuid>
#include <optional>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace
{
KConfigGroup generalGroup()
{
    return KSharedConfig::openConfig(u"plasma-assistrc"_s)->group(u"General"_s);
}

KConfigGroup confirmationGroup()
{
    return KSharedConfig::openConfig(u"plasma-assistrc"_s)->group(u"Confirmation"_s);
}

// Same rules as server: language models often send numbers and booleans as strings.
// Value is shown as received when it's not valid: user sees what server will refuse
QString integerText(const QJsonValue &value)
{
    if (value.isDouble()) {
        return QString::number(value.toInteger());
    }
    return value.toString().trimmed();
}

std::optional<bool> booleanValue(const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool();
    }
    if (const QString text = value.toString().trimmed().toLower(); text == "true"_L1 || text == "false"_L1) {
        return text == "true"_L1;
    }
    return std::nullopt;
}

QString numberText(const QJsonValue &value)
{
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    return value.toString().trimmed();
}

bool hasValue(const QJsonObject &arguments, QLatin1StringView name)
{
    const QJsonValue value = arguments.value(name);
    return !value.isUndefined() && !value.isNull();
}

QString displayChanges(const QJsonObject &arguments)
{
    QStringList changes;
    if (hasValue(arguments, "enabled"_L1)) {
        const std::optional<bool> enabled = booleanValue(arguments.value("enabled"_L1));
        changes.append(!enabled ? arguments.value("enabled"_L1).toString()
                                : (*enabled ? i18nc("@info display change", "enable") : i18nc("@info display change", "disable")));
    }
    if (hasValue(arguments, "resolution"_L1)) {
        changes.append(i18nc("@info display change", "resolution %1", arguments.value("resolution"_L1).toString()));
    }
    if (hasValue(arguments, "refresh_rate"_L1)) {
        changes.append(i18nc("@info display change", "refresh rate %1 Hz", numberText(arguments.value("refresh_rate"_L1))));
    }
    if (hasValue(arguments, "scale"_L1)) {
        changes.append(i18nc("@info display change", "scale %1", numberText(arguments.value("scale"_L1))));
    }
    if (hasValue(arguments, "x"_L1) || hasValue(arguments, "y"_L1)) {
        const QString unchanged = i18nc("@info display position not changed", "unchanged");
        changes.append(i18nc("@info display change",
                             "position %1,%2",
                             hasValue(arguments, "x"_L1) ? integerText(arguments.value("x"_L1)) : unchanged,
                             hasValue(arguments, "y"_L1) ? integerText(arguments.value("y"_L1)) : unchanged));
    }
    return changes.join(i18nc("@info separator of display changes", ", "));
}

QString deviceOrDefault(const QJsonObject &arguments)
{
    const QString device = arguments.value("device"_L1).toString();
    return device.isEmpty() ? i18nc("@info audio device", "default device") : device;
}
}

ConfirmationManager::ConfirmationManager(QObject *parent)
    : QObject{parent}
    , mClientWatcher(new QDBusServiceWatcher({}, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration, this))
    , mTimeout(120s)
{
    mActionsAllowed = generalGroup().readEntry("AllowActions", false);
    mAlwaysAllowedTools = confirmationGroup().readEntry("AlwaysAllowedTools", QStringList());
    // No client can answer anymore
    connect(mClientWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &client) {
        mClientWatcher->removeWatchedService(client);
        const QList<QString> identifiers = mPendingRequests.keys();
        for (const QString &identifier : identifiers) {
            if (mPendingRequests.value(identifier).request.client == client) {
                finish(identifier, false);
            }
        }
    });
}

ConfirmationManager::~ConfirmationManager() = default;

bool ConfirmationManager::actionsAllowed() const
{
    return mActionsAllowed;
}

void ConfirmationManager::setActionsAllowed(bool allowed)
{
    if (mActionsAllowed == allowed) {
        return;
    }
    mActionsAllowed = allowed;
    KConfigGroup group = generalGroup();
    group.writeEntry("AllowActions", allowed);
    group.sync();
    Q_EMIT actionsAllowedChanged();
}

void ConfirmationManager::setTimeout(std::chrono::milliseconds timeout)
{
    mTimeout = timeout;
}

bool ConfirmationManager::isAlwaysAllowed(const QString &tool) const
{
    return mAlwaysAllowedTools.contains(tool);
}

QStringList ConfirmationManager::alwaysAllowedTools() const
{
    return mAlwaysAllowedTools;
}

void ConfirmationManager::setAlwaysAllowedTools(const QStringList &tools)
{
    mAlwaysAllowedTools = tools;
    saveAlwaysAllowedTools();
}

void ConfirmationManager::saveAlwaysAllowedTools() const
{
    KConfigGroup group = confirmationGroup();
    group.writeEntry("AlwaysAllowedTools", mAlwaysAllowedTools);
    group.sync();
}

bool ConfirmationManager::hasPendingRequest() const
{
    return !mPendingRequests.isEmpty();
}

QString ConfirmationManager::riskName(Risk risk)
{
    switch (risk) {
    case Risk::Modify:
        return u"modify"_s;
    case Risk::Destructive:
        break;
    case Risk::Keep:
        return u"keep"_s;
    }
    return u"destructive"_s;
}

QString ConfirmationManager::summary(const QString &tool, const QString &title, const QJsonObject &arguments)
{
    const bool source = arguments.value("kind"_L1).toString() == "source"_L1;
    if (tool == "set_default_audio_device"_L1) {
        return source ? i18nc("@info confirmation", "Use %1 as default input", arguments.value("device"_L1).toString())
                      : i18nc("@info confirmation", "Use %1 as default output", arguments.value("device"_L1).toString());
    }
    if (tool == "set_audio_volume"_L1) {
        return i18nc("@info confirmation", "Set volume of %1 to %2%", deviceOrDefault(arguments), integerText(arguments.value("volume_percent"_L1)));
    }
    if (tool == "set_audio_mute"_L1) {
        if (const std::optional<bool> muted = booleanValue(arguments.value("muted"_L1)); muted) {
            return *muted ? i18nc("@info confirmation", "Mute %1", deviceOrDefault(arguments))
                          : i18nc("@info confirmation", "Unmute %1", deviceOrDefault(arguments));
        }
    }
    if (tool == "set_card_profile"_L1) {
        return i18nc("@info confirmation", "Change profile of %1 to %2", arguments.value("card"_L1).toString(), arguments.value("profile"_L1).toString());
    }
    if (tool == "move_audio_stream"_L1) {
        return i18nc("@info confirmation", "Move sound of stream %1 to %2", integerText(arguments.value("stream"_L1)), arguments.value("device"_L1).toString());
    }
    if (tool == "set_wifi_enabled"_L1) {
        if (const std::optional<bool> enabled = booleanValue(arguments.value("enabled"_L1)); enabled) {
            return *enabled ? i18nc("@info confirmation", "Enable Wi-Fi") : i18nc("@info confirmation", "Disable Wi-Fi");
        }
    }
    if (tool == "reconnect_network"_L1) {
        return i18nc("@info confirmation", "Reconnect network connection %1", arguments.value("connection"_L1).toString());
    }
    if (tool == "set_power_profile"_L1) {
        return i18nc("@info confirmation", "Set power profile to %1", arguments.value("profile"_L1).toString());
    }
    if (tool == "restart_plasmashell"_L1) {
        return i18nc("@info confirmation", "Restart Plasma shell: panels and desktop are reloaded");
    }
    if (tool == "set_output_config"_L1) {
        return i18nc("@info confirmation, %1 display, %2 changes",
                     "Change display %1: %2. You will be asked to keep the new settings, otherwise they are reverted.",
                     arguments.value("output"_L1).toString(),
                     displayChanges(arguments));
    }
    if (tool == "undo_change"_L1) {
        return i18nc("@info confirmation", "Undo change %1", arguments.value("id"_L1).toString());
    }
    // Unknown tool: all arguments are shown
    return i18nc("@info confirmation, %1 tool, %2 arguments", "%1 with %2", title, QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact)));
}

void ConfirmationManager::request(const Request &request, const AnswerCallback &callback)
{
    // Change to keep was already allowed
    if (request.risk != Risk::Keep) {
        if (!mActionsAllowed) {
            Q_EMIT actionRefused(request.conversation, request.title);
            callback(false);
            return;
        }
        if (request.risk == Risk::Modify && isAlwaysAllowed(request.tool)) {
            callback(true);
            return;
        }
    }
    if (!request.client.isEmpty()) {
        if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(request.client)) {
            qCDebug(PLASMA_ASSIST_DAEMON_LOG) << "No client to confirm" << request.tool;
            callback(false);
            return;
        }
        mClientWatcher->addWatchedService(request.client);
    }
    const QString identifier = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, identifier]() {
        qCDebug(PLASMA_ASSIST_DAEMON_LOG) << "Confirmation was not answered:" << identifier;
        finish(identifier, false);
    });
    mPendingRequests.insert(identifier, PendingRequest{.request = request, .callback = callback, .timer = timer});
    timer->start(request.timeout > 0ms ? request.timeout : mTimeout);
    Q_EMIT confirmationRequested(identifier,
                                 request.conversation,
                                 request.tool,
                                 request.risk == Risk::Keep ? request.title : summary(request.tool, request.title, request.arguments),
                                 QString::fromUtf8(QJsonDocument(request.arguments).toJson(QJsonDocument::Compact)),
                                 riskName(request.risk));
}

bool ConfirmationManager::confirm(const QString &requestIdentifier, Answer answer, const QString &caller)
{
    const auto it = mPendingRequests.constFind(requestIdentifier);
    if (it == mPendingRequests.cend()) {
        return false;
    }
    const Request &request = it->request;
    // Another process can't answer instead of user
    if (!request.client.isEmpty() && caller != request.client) {
        qCWarning(PLASMA_ASSIST_DAEMON_LOG) << "Confirmation answered by another client:" << caller;
        return false;
    }
    if (answer == Answer::AlwaysAllow && request.risk == Risk::Modify && !mAlwaysAllowedTools.contains(request.tool)) {
        mAlwaysAllowedTools.append(request.tool);
        saveAlwaysAllowedTools();
    }
    finish(requestIdentifier, answer != Answer::Deny);
    return true;
}

void ConfirmationManager::finish(const QString &requestIdentifier, bool allowed)
{
    const PendingRequest pending = mPendingRequests.take(requestIdentifier);
    if (!pending.callback) {
        return;
    }
    pending.timer->deleteLater();
    Q_EMIT confirmationFinished(requestIdentifier, allowed);
    pending.callback(allowed);
}

#include "moc_confirmationmanager.cpp"
