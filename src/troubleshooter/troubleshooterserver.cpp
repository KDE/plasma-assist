/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "troubleshooterserver.h"
#include "config-plasma-assist.h"
#include "modules/systemmodule.h"
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolImplementation>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

TroubleshooterServer::TroubleshooterServer(QObject *parent)
    : McpProtocolServerProtocolManager(McpProtocolPlugin::TransportType::Stdio, parent)
{
    McpProtocolImplementation serverInfo;
    serverInfo.setName(u"plasma-assist-troubleshooter"_s);
    serverInfo.setVersion(QStringLiteral(PLASMA_ASSIST_VERSION));
    setServerInfo(serverInfo);
    setInstructions(u"Tools to diagnose and fix problems on KDE Plasma desktop. Always diagnose before proposing an action."_s);

    SystemModule::registerTools(this);
}

TroubleshooterServer::~TroubleshooterServer() = default;

#include "moc_troubleshooterserver.cpp"
