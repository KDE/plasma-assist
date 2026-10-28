/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "troubleshooterserver.h"
#include "backends/commandrunner.h"
#include "backends/kscreenbackend.h"
#include "backends/networkmanagerbackend.h"
#include "backends/pulseaudiobackend.h"
#include "backends/systempowerbackend.h"
#include "config-plasma-assist.h"
#include "core/actions.h"
#include "core/changejournal.h"
#include "core/privacyfilter.h"
#include "modules/audioactions.h"
#include "modules/audiomodule.h"
#include "modules/bugreportmodule.h"
#include "modules/crashmodule.h"
#include "modules/displayactions.h"
#include "modules/displaymodule.h"
#include "modules/logmodule.h"
#include "modules/networkmodule.h"
#include "modules/powermodule.h"
#include "modules/reportmodule.h"
#include "modules/systemactions.h"
#include "modules/systemmodule.h"
#include "prompts/scenarioprompts.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolImplementation>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

TroubleshooterServer::TroubleshooterServer(QObject *parent)
    : McpProtocolServerProtocolManager(McpProtocolPlugin::TransportType::Stdio, parent)
    , mAudioBackend(std::make_unique<PulseAudioBackend>())
    , mDisplayBackend(std::make_unique<KScreenBackend>())
    , mNetworkBackend(std::make_unique<NetworkManagerBackend>())
    , mPowerBackend(std::make_unique<SystemPowerBackend>())
    , mCommandRunner(std::make_unique<ProcessCommandRunner>())
    , mChangeJournal(std::make_unique<ChangeJournal>())
    , mActionRunner(std::make_unique<ActionRunner>(mChangeJournal.get()))
{
    McpProtocolImplementation serverInfo;
    serverInfo.setName(u"plasma-assist-troubleshooter"_s);
    serverInfo.setVersion(QStringLiteral(PLASMA_ASSIST_VERSION));
    setServerInfo(serverInfo);
    setInstructions(
        u"Tools to diagnose and fix problems on KDE Plasma desktop. Always diagnose before proposing an action. "
        "Personal data in results (addresses, user name, network names) are replaced by pseudonyms like <ip-1> or <name-1>."_s);

    // Personal data are masked in results: language model can be a cloud provider
    PrivacyFilter *filter = PrivacyFilter::instance();
    filter->setEnabled(KSharedConfig::openConfig(u"plasma-assistrc"_s)->group(u"Privacy"_s).readEntry("FilterPersonalData", true));
    filter->addWordsProvider([backend = mNetworkBackend.get()]() {
        return backend->isAvailable() ? backend->sensitiveNames() : QStringList();
    });

    SystemModule::registerTools(this);
    AudioModule::registerTools(this, mAudioBackend.get());
    DisplayModule::registerTools(this, mDisplayBackend.get());
    NetworkModule::registerTools(this, mNetworkBackend.get());
    PowerModule::registerTools(this, mPowerBackend.get());
    CrashModule::registerTools(this, mCommandRunner.get());
    LogModule::registerTools(this, mCommandRunner.get());
    // Use tools above
    ReportModule::registerTools(this);
    BugReportModule::registerTools(this);
    ScenarioPrompts::registerPrompts(this);

    // Actions: client asks user to confirm them (annotations)
    AudioActions::registerTools(this, mAudioBackend.get(), mActionRunner.get());
    DisplayActions::registerTools(this, mDisplayBackend.get(), mActionRunner.get());
    SystemActions::registerTools(this, mNetworkBackend.get(), mPowerBackend.get(), mCommandRunner.get(), mActionRunner.get());
    mActionRunner->registerTools(this);
}

TroubleshooterServer::~TroubleshooterServer() = default;

void TroubleshooterServer::revertPendingChanges(const std::function<void()> &callback)
{
    mActionRunner->revertPendingChanges(callback);
}

#include "moc_troubleshooterserver.cpp"
