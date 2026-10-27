/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systempowerbackend.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QMetaEnum>
#include <Solid/Battery>
#include <Solid/Device>

using namespace Qt::Literals::StringLiterals;

namespace
{
const QString powerDevilService = u"org.kde.Solid.PowerManagement"_s;

template<typename Enum>
QString enumName(Enum value)
{
    return QString::fromLatin1(QMetaEnum::fromType<Enum>().valueToKey(value));
}

// Calls \a callback with reply of \a message, it's not called if \a context is deleted
void callDBus(const QDBusConnection &bus, const QDBusMessage &message, QObject *context, const std::function<void(const QDBusMessage &reply)> &callback)
{
    auto watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [callback](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        callback(call->reply());
    });
}

QDBusMessage propertyMessage(const QString &service, const QString &path, const QString &interface, const QString &property)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, u"org.freedesktop.DBus.Properties"_s, u"Get"_s);
    message.setArguments({interface, property});
    return message;
}

QList<BatteryInfo> batteries()
{
    QList<BatteryInfo> result;
    const QList<Solid::Device> devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery);
    for (const Solid::Device &device : devices) {
        const auto battery = device.as<Solid::Battery>();
        if (!battery) {
            continue;
        }
        result.append({
            .name = device.product().isEmpty() ? device.description() : device.product(),
            .type = enumName(battery->type()),
            .chargeState = enumName(battery->chargeState()),
            .chargePercent = battery->chargePercent(),
            .capacityPercent = battery->capacity(),
            .timeToEmptySec = battery->timeToEmpty(),
            .present = battery->isPresent(),
            .powerSupply = battery->isPowerSupply(),
        });
    }
    return result;
}

// Settings saved by Power Management settings module (default values are not saved)
QMap<QString, QMap<QString, QString>> powerSettings()
{
    QMap<QString, QMap<QString, QString>> result;
    const KSharedConfig::Ptr config = KSharedConfig::openConfig(u"powerdevilrc"_s, KConfig::SimpleConfig);
    for (const QString &profile : {u"AC"_s, u"Battery"_s, u"LowBattery"_s}) {
        const KConfigGroup profileGroup = config->group(profile);
        const QStringList groups = profileGroup.groupList();
        for (const QString &group : groups) {
            if (const QMap<QString, QString> entries = profileGroup.group(group).entryMap(); !entries.isEmpty()) {
                result.insert(profile + u'/' + group, entries);
            }
        }
    }
    return result;
}

// Reads a(ssssu) of PowerDevil or a(ssssuu) of logind
QList<SleepInhibitor> readInhibitors(const QDBusArgument &argument, const QString &source)
{
    QList<SleepInhibitor> result;
    argument.beginArray();
    while (!argument.atEnd()) {
        SleepInhibitor inhibitor;
        inhibitor.source = source;
        uint flagsOrUid = 0;
        argument.beginStructure();
        argument >> inhibitor.what >> inhibitor.who >> inhibitor.why >> inhibitor.mode >> flagsOrUid;
        if (source == "logind"_L1) {
            argument >> inhibitor.pid;
        }
        argument.endStructure();
        result.append(inhibitor);
    }
    argument.endArray();
    return result;
}

// Status is sent when all D-Bus answers are received
struct StatusRequest {
    PowerStatus status;
    int pending = 3;
};
}

SystemPowerBackend::SystemPowerBackend() = default;

SystemPowerBackend::~SystemPowerBackend() = default;

void SystemPowerBackend::fetchStatus(QObject *context, const StatusCallback &callback)
{
    auto request = std::make_shared<StatusRequest>();
    request->status.batteries = batteries();
    request->status.settings = powerSettings();
    const auto done = [request, callback]() {
        if (--request->pending == 0) {
            callback(request->status, {});
        }
    };
    // Errors are ignored: UPower or power profiles can be missing
    callDBus(QDBusConnection::systemBus(),
             propertyMessage(u"org.freedesktop.UPower"_s, u"/org/freedesktop/UPower"_s, u"org.freedesktop.UPower"_s, u"OnBattery"_s),
             context,
             [request, done](const QDBusMessage &reply) {
                 if (reply.type() == QDBusMessage::ReplyMessage) {
                     request->status.onBattery = reply.arguments().value(0).value<QDBusVariant>().variant().toBool();
                 }
                 done();
             });
    const QString profilePath = u"/org/kde/Solid/PowerManagement/Actions/PowerProfile"_s;
    const QString profileInterface = u"org.kde.Solid.PowerManagement.Actions.PowerProfile"_s;
    callDBus(QDBusConnection::sessionBus(),
             QDBusMessage::createMethodCall(powerDevilService, profilePath, profileInterface, u"currentProfile"_s),
             context,
             [request, done](const QDBusMessage &reply) {
                 if (reply.type() == QDBusMessage::ReplyMessage) {
                     request->status.powerProfile = reply.arguments().value(0).toString();
                 }
                 done();
             });
    callDBus(QDBusConnection::sessionBus(),
             QDBusMessage::createMethodCall(powerDevilService, profilePath, profileInterface, u"profileChoices"_s),
             context,
             [request, done](const QDBusMessage &reply) {
                 if (reply.type() == QDBusMessage::ReplyMessage) {
                     request->status.powerProfileChoices = reply.arguments().value(0).toStringList();
                 }
                 done();
             });
}

void SystemPowerBackend::fetchInhibitors(QObject *context, const InhibitorsCallback &callback)
{
    struct InhibitorsRequest {
        QList<SleepInhibitor> inhibitors;
        QStringList errors;
        int pending = 2;
    };
    auto request = std::make_shared<InhibitorsRequest>();
    const auto done = [request, callback]() {
        if (--request->pending > 0) {
            return;
        }
        // Error only when nothing can be read
        callback(request->inhibitors, request->errors.count() == 2 ? request->errors.join(u", "_s) : QString());
    };
    callDBus(QDBusConnection::sessionBus(),
             propertyMessage(powerDevilService,
                             u"/org/kde/Solid/PowerManagement/PolicyAgent"_s,
                             u"org.kde.Solid.PowerManagement.PolicyAgent"_s,
                             u"ActiveInhibitions"_s),
             context,
             [request, done](const QDBusMessage &reply) {
                 if (reply.type() == QDBusMessage::ReplyMessage) {
                     const QDBusArgument argument = reply.arguments().value(0).value<QDBusVariant>().variant().value<QDBusArgument>();
                     request->inhibitors += readInhibitors(argument, u"powerdevil"_s);
                 } else {
                     request->errors.append(reply.errorMessage());
                 }
                 done();
             });
    callDBus(
        QDBusConnection::systemBus(),
        QDBusMessage::createMethodCall(u"org.freedesktop.login1"_s, u"/org/freedesktop/login1"_s, u"org.freedesktop.login1.Manager"_s, u"ListInhibitors"_s),
        context,
        [request, done](const QDBusMessage &reply) {
            if (reply.type() == QDBusMessage::ReplyMessage) {
                request->inhibitors += readInhibitors(reply.arguments().value(0).value<QDBusArgument>(), u"logind"_s);
            } else {
                request->errors.append(reply.errorMessage());
            }
            done();
        });
}

void SystemPowerBackend::setPowerProfile(const QString &profile, QObject *context, const ErrorCallback &callback)
{
    QDBusMessage message = QDBusMessage::createMethodCall(powerDevilService,
                                                          u"/org/kde/Solid/PowerManagement/Actions/PowerProfile"_s,
                                                          u"org.kde.Solid.PowerManagement.Actions.PowerProfile"_s,
                                                          u"setProfile"_s);
    message.setArguments({profile});
    callDBus(QDBusConnection::sessionBus(), message, context, [callback](const QDBusMessage &reply) {
        callback(reply.type() == QDBusMessage::ErrorMessage ? reply.errorMessage() : QString());
    });
}
