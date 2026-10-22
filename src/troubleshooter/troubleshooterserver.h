/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>

// MCP server (stdio) which provides tools to diagnose and fix KDE Plasma desktop
class TroubleshooterServer : public TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager
{
    Q_OBJECT
public:
    explicit TroubleshooterServer(QObject *parent = nullptr);
    ~TroubleshooterServer() override;
};
