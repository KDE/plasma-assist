/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "troubleshooterserver.h"
#include "backends/kscreenbackend.h"
#include "backends/pulseaudiobackend.h"
#include "config-plasma-assist.h"
#include "modules/audiomodule.h"
#include "modules/displaymodule.h"
#include "modules/systemmodule.h"
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolImplementation>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

TroubleshooterServer::TroubleshooterServer(QObject *parent)
    : McpProtocolServerProtocolManager(McpProtocolPlugin::TransportType::Stdio, parent)
    , mAudioBackend(std::make_unique<PulseAudioBackend>())
    , mDisplayBackend(std::make_unique<KScreenBackend>())
{
    McpProtocolImplementation serverInfo;
    serverInfo.setName(u"plasma-assist-troubleshooter"_s);
    serverInfo.setVersion(QStringLiteral(PLASMA_ASSIST_VERSION));
    setServerInfo(serverInfo);
    setInstructions(u"Tools to diagnose and fix problems on KDE Plasma desktop. Always diagnose before proposing an action."_s);

    SystemModule::registerTools(this);
    AudioModule::registerTools(this, mAudioBackend.get());
    DisplayModule::registerTools(this, mDisplayBackend.get());
}

TroubleshooterServer::~TroubleshooterServer() = default;

#include "moc_troubleshooterserver.cpp"
