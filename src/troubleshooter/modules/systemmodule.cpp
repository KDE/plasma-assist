/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systemmodule.h"
#include "core/toolhelpers.h"
#include <KCoreAddons>
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QSysInfo>
#include <QtConcurrentRun>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
QString readFirstLine(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readLine()).trimmed();
}

// One entry by graphic card: kernel driver and PCI identifiers (no serial number)
QJsonArray graphicCards()
{
    QJsonArray cards;
    const QDir drmDir(u"/sys/class/drm"_s);
    const QStringList entries = drmDir.entryList({u"card*"_s}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &entry : entries) {
        // card0-HDMI-A-1 is a connector, not a card
        if (entry.contains(u'-')) {
            continue;
        }
        const QString devicePath = drmDir.filePath(entry + u"/device"_s);
        cards.append(QJsonObject{
            {u"card"_s, entry},
            {u"driver"_s, QFileInfo(devicePath + u"/driver"_s).symLinkTarget().section(u'/', -1)},
            {u"vendorId"_s, readFirstLine(devicePath + u"/vendor"_s)},
            {u"deviceId"_s, readFirstLine(devicePath + u"/device"_s)},
        });
    }
    return cards;
}
}

QJsonObject SystemModule::parseOsRelease(const QByteArray &content)
{
    static const QStringList keys{u"NAME"_s, u"PRETTY_NAME"_s, u"ID"_s, u"VERSION_ID"_s, u"BUILD_ID"_s, u"VARIANT"_s};
    QJsonObject distribution;
    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray &line : lines) {
        const qsizetype separator = line.indexOf('=');
        if (separator <= 0 || line.startsWith('#')) {
            continue;
        }
        const QString key = QString::fromUtf8(line.left(separator)).trimmed();
        if (!keys.contains(key)) {
            continue;
        }
        QString value = QString::fromUtf8(line.mid(separator + 1)).trimmed();
        if (value.size() >= 2 && (value.startsWith(u'"') || value.startsWith(u'\'')) && value.endsWith(value.at(0))) {
            value = value.mid(1, value.size() - 2);
        }
        distribution.insert(key, value);
    }
    return distribution;
}

QJsonObject SystemModule::systemInfo()
{
    QByteArray osRelease;
    for (const QString &fileName : {u"/etc/os-release"_s, u"/usr/lib/os-release"_s}) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            osRelease = file.readAll();
            break;
        }
    }
    return QJsonObject{
        {u"distribution"_s, parseOsRelease(osRelease)},
        {u"kernel"_s, QJsonObject{{u"type"_s, QSysInfo::kernelType()}, {u"version"_s, QSysInfo::kernelVersion()}}},
        {u"architecture"_s, QSysInfo::currentCpuArchitecture()},
        {u"qtVersion"_s, QString::fromLatin1(qVersion())},
        {u"frameworksVersion"_s, KCoreAddons::versionString()},
        {u"session"_s,
         QJsonObject{
             {u"type"_s, qEnvironmentVariable("XDG_SESSION_TYPE")},
             {u"desktop"_s, qEnvironmentVariable("XDG_CURRENT_DESKTOP")},
             {u"kdeSessionVersion"_s, qEnvironmentVariable("KDE_SESSION_VERSION")},
         }},
        {u"graphicCards"_s, graphicCards()},
    };
}

bool SystemModule::isLowSpace(qint64 totalBytes, qint64 availableBytes)
{
    return totalBytes > 0 && availableBytes * 100 < totalBytes * 5;
}

bool SystemModule::isPhysicalVolume(const QStorageInfo &storage)
{
    static const QList<QByteArray> virtualFileSystems{"tmpfs", "devtmpfs", "ramfs", "squashfs", "overlay", "efivarfs", "fuse.portal", "fuse.gvfsd-fuse"};
    return storage.isValid() && storage.isReady() && !virtualFileSystems.contains(storage.fileSystemType());
}

QJsonObject SystemModule::volumeInfo(const QStorageInfo &storage)
{
    const qint64 total = storage.bytesTotal();
    const qint64 available = storage.bytesAvailable();
    return QJsonObject{
        {u"mountPoint"_s, storage.rootPath()},
        {u"device"_s, QString::fromUtf8(storage.device())},
        {u"fileSystem"_s, QString::fromUtf8(storage.fileSystemType())},
        {u"readOnly"_s, storage.isReadOnly()},
        {u"totalBytes"_s, total},
        {u"availableBytes"_s, available},
        {u"availablePercent"_s, total > 0 ? qRound(available * 1000.0 / total) / 10.0 : 0.0},
        {u"lowSpace"_s, isLowSpace(total, available)},
    };
}

QJsonArray SystemModule::mergeVolumesByDevice(const QJsonArray &volumes)
{
    QStringList devices;
    QList<QJsonObject> merged;
    for (const QJsonValue &value : volumes) {
        const QJsonObject volume = value.toObject();
        const QString device = volume.value("device"_L1).toString();
        const qsizetype index = devices.indexOf(device);
        if (index < 0) {
            devices.append(device);
            merged.append(volume);
            continue;
        }
        QJsonObject &existing = merged[index];
        QJsonArray otherMountPoints = existing.value("otherMountPoints"_L1).toArray();
        const QString mountPoint = volume.value("mountPoint"_L1).toString();
        // Shortest mount point is the main one (/ instead of /home)
        if (mountPoint.size() < existing.value("mountPoint"_L1).toString().size()) {
            otherMountPoints.append(existing.value("mountPoint"_L1));
            existing.insert("mountPoint"_L1, mountPoint);
        } else {
            otherMountPoints.append(mountPoint);
        }
        existing.insert("otherMountPoints"_L1, otherMountPoints);
    }
    QJsonArray result;
    for (const QJsonObject &volume : std::as_const(merged)) {
        result.append(volume);
    }
    return result;
}

QJsonObject SystemModule::diskUsage()
{
    QJsonArray volumes;
    const QList<QStorageInfo> storages = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &storage : storages) {
        if (isPhysicalVolume(storage)) {
            volumes.append(volumeInfo(storage));
        }
    }
    return QJsonObject{{u"volumes"_s, mergeVolumesByDevice(volumes)}};
}

void SystemModule::registerTools(McpProtocolServerProtocolManager *manager)
{
    const ToolHelpers::ToolGroup group{u"system"_s, i18nc("@title tools group", "System")};

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(u"get_system_info"_s,
                                i18nc("@title tool", "System Information"),
                                u"Return distribution, kernel, Qt and KDE Frameworks versions, session type (Wayland or X11), "
                                "desktop and graphic cards with their kernel driver. Use it first to know the environment."_s,
                                ToolHelpers::ToolKind::Diagnostic,
                                group),
        [](McpProtocolServerToolCall *call) {
            const QJsonObject info = systemInfo();
            const QJsonObject distribution = info.value("distribution"_L1).toObject();
            const QJsonObject session = info.value("session"_L1).toObject();
            ToolHelpers::finishWithJson(call,
                                        info,
                                        u"%1, kernel %2, KDE Frameworks %3, %4 session"_s.arg(distribution.value("PRETTY_NAME"_L1).toString(),
                                                                                              info.value("kernel"_L1).toObject().value("version"_L1).toString(),
                                                                                              info.value("frameworksVersion"_L1).toString(),
                                                                                              session.value("type"_L1).toString()));
        });

    ToolHelpers::addFunctionTool(
        manager,
        ToolHelpers::createTool(u"get_disk_usage"_s,
                                i18nc("@title tool", "Disk Usage"),
                                u"Return mounted physical volumes with size, available space and lowSpace=true when less than 5% is available. "
                                "Use it when applications fail to save files or the system is slow."_s,
                                ToolHelpers::ToolKind::Diagnostic,
                                group),
        [](McpProtocolServerToolCall *call) {
            // statfs() can block on network file systems
            auto watcher = new QFutureWatcher<QJsonObject>(call);
            QObject::connect(watcher, &QFutureWatcher<QJsonObject>::finished, call, [call, watcher]() {
                const QJsonObject usage = watcher->result();
                const QJsonArray volumes = usage.value("volumes"_L1).toArray();
                QStringList lowSpaceVolumes;
                for (const QJsonValue &volume : volumes) {
                    if (volume["lowSpace"_L1].toBool()) {
                        lowSpaceVolumes.append(volume["mountPoint"_L1].toString());
                    }
                }
                const QString summary = lowSpaceVolumes.isEmpty() ? u"%1 volumes, none is low on space"_s.arg(volumes.count())
                                                                  : u"%1 volumes, low on space: %2"_s.arg(volumes.count()).arg(lowSpaceVolumes.join(u", "_s));
                ToolHelpers::finishWithJson(call, usage, summary);
            });
            watcher->setFuture(QtConcurrent::run(&SystemModule::diskUsage));
        });
}
