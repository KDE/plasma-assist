/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "backends/audiobackend.h"
#include <QJsonObject>
#include <QStringList>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Tools list_audio_devices, list_audio_cards and list_audio_streams
namespace AudioModule
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, AudioBackend *backend);

// kind: sink, source or all
[[nodiscard]] QJsonObject devicesToJson(const AudioState &state, const QString &kind);
[[nodiscard]] QJsonObject cardsToJson(const AudioState &state);
[[nodiscard]] QJsonObject streamsToJson(const AudioState &state);

// Obvious problems (muted default output, headphones plugged on another output…): helps small language models
[[nodiscard]] QStringList hints(const AudioState &state);
}
