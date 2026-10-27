/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/actions.h"
#include <QList>
#include <utility>

class AudioBackend;
namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
class McpProtocolTool;
}

// Tools set_default_audio_device, set_audio_volume, set_audio_mute, set_card_profile and move_audio_stream
namespace AudioActions
{
void registerTools(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager, AudioBackend *backend, ActionRunner *runner);

// Definitions of tools and their actions
[[nodiscard]] QList<std::pair<TextAutoGenerateTextMcpProtocolCore::McpProtocolTool, Action>> actions(AudioBackend *backend);
}
