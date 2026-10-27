/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <memory>

class ActionRunner;
class AudioBackend;
class ChangeJournal;
class CommandRunner;
class DisplayBackend;
class NetworkBackend;
class PowerBackend;

// MCP server (stdio) which provides tools to diagnose and fix KDE Plasma desktop
class TroubleshooterServer : public TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager
{
    Q_OBJECT
public:
    explicit TroubleshooterServer(QObject *parent = nullptr);
    ~TroubleshooterServer() override;

private:
    // Used by tools. Deleted before base class (tools): no tool is called during destruction
    const std::unique_ptr<AudioBackend> mAudioBackend;
    const std::unique_ptr<DisplayBackend> mDisplayBackend;
    const std::unique_ptr<NetworkBackend> mNetworkBackend;
    const std::unique_ptr<PowerBackend> mPowerBackend;
    const std::unique_ptr<CommandRunner> mCommandRunner;
    const std::unique_ptr<ChangeJournal> mChangeJournal;
    const std::unique_ptr<ActionRunner> mActionRunner;
};
