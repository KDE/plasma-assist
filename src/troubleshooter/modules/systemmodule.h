/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStorageInfo>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tools get_system_info and get_disk_usage
namespace SystemModule
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager);

// Parses content of /etc/os-release (see os-release(5))
[[nodiscard]] QJsonObject parseOsRelease(const QByteArray &content);
[[nodiscard]] QJsonObject systemInfo();

// Volume is low on space when less than 5% is available
[[nodiscard]] bool isLowSpace(qint64 totalBytes, qint64 availableBytes);
[[nodiscard]] bool isPhysicalVolume(const QStorageInfo &storage);
[[nodiscard]] QJsonObject volumeInfo(const QStorageInfo &storage);
// Subvolumes (btrfs) and bind mounts are listed once, with their otherMountPoints
[[nodiscard]] QJsonArray mergeVolumesByDevice(const QJsonArray &volumes);
// Can block (network file systems): don't call it in main thread
[[nodiscard]] QJsonObject diskUsage();
}
