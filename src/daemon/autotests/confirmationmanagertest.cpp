/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "confirmationmanagertest.h"
#include "confirmationmanager.h"
#include <QDBusConnection>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
QTEST_GUILESS_MAIN(ConfirmationManagerTest)

namespace
{
ConfirmationManager::Request request(const QString &tool, ConfirmationManager::Risk risk = ConfirmationManager::Risk::Modify, const QString &client = {})
{
    return ConfirmationManager::Request{
        .tool = tool,
        .title = u"Mute or Unmute"_s,
        .arguments = {{u"muted"_s, true}},
        .risk = risk,
        .conversation = "conversation"_ba,
        .client = client,
    };
}

// Answer given to callback of request (-1: not called yet)
struct Answer {
    int value = -1;
    ConfirmationManager::AnswerCallback callback()
    {
        return [this](bool allowed) noexcept {
            value = allowed ? 1 : 0;
        };
    }
};
}

ConfirmationManagerTest::ConfirmationManagerTest(QObject *parent)
    : QObject{parent}
{
}

void ConfirmationManagerTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ConfirmationManagerTest::init()
{
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/plasma-assistrc"_s);
}

void ConfirmationManagerTest::shouldRefuseWhenActionsAreDisabled()
{
    ConfirmationManager manager;
    QVERIFY(!manager.actionsAllowed());
    QSignalSpy requested(&manager, &ConfirmationManager::confirmationRequested);
    QSignalSpy refused(&manager, &ConfirmationManager::actionRefused);
    Answer answer;
    manager.request(request(u"set_audio_mute"_s), answer.callback());
    QCOMPARE(answer.value, 0);
    QCOMPARE(requested.count(), 0);
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.at(0).at(1).toString(), u"Mute or Unmute"_s);
}

void ConfirmationManagerTest::shouldAskUser()
{
    {
        ConfirmationManager manager;
        manager.setActionsAllowed(true);
    }
    // Setting is saved
    ConfirmationManager manager;
    QVERIFY(manager.actionsAllowed());
    QSignalSpy requested(&manager, &ConfirmationManager::confirmationRequested);
    QSignalSpy finished(&manager, &ConfirmationManager::confirmationFinished);
    Answer answer;
    manager.request(request(u"set_audio_mute"_s), answer.callback());
    QCOMPARE(answer.value, -1);
    QCOMPARE(requested.count(), 1);
    QVERIFY(manager.hasPendingRequest());
    const QString identifier = requested.at(0).at(0).toString();
    QCOMPARE(requested.at(0).at(1).toByteArray(), "conversation"_ba);
    QCOMPARE(requested.at(0).at(3).toString(), u"Mute default device"_s);
    QCOMPARE(requested.at(0).at(5).toString(), u"modify"_s);

    QVERIFY(manager.confirm(identifier, ConfirmationManager::Answer::Allow, {}));
    QCOMPARE(answer.value, 1);
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.at(0).at(1).toBool());
    QVERIFY(!manager.hasPendingRequest());
    // Request is answered once
    QVERIFY(!manager.confirm(identifier, ConfirmationManager::Answer::Deny, {}));
}

void ConfirmationManagerTest::shouldDenyWhenUserRefuses()
{
    ConfirmationManager manager;
    manager.setActionsAllowed(true);
    QSignalSpy requested(&manager, &ConfirmationManager::confirmationRequested);
    Answer answer;
    manager.request(request(u"set_audio_mute"_s), answer.callback());
    QVERIFY(manager.confirm(requested.at(0).at(0).toString(), ConfirmationManager::Answer::Deny, {}));
    QCOMPARE(answer.value, 0);
}

void ConfirmationManagerTest::shouldRejectAnswerOfAnotherClient()
{
    const QString client = QDBusConnection::sessionBus().baseService();
    if (client.isEmpty()) {
        QSKIP("No session bus");
    }
    ConfirmationManager manager;
    manager.setActionsAllowed(true);
    QSignalSpy requested(&manager, &ConfirmationManager::confirmationRequested);
    Answer answer;
    manager.request(request(u"set_audio_mute"_s, ConfirmationManager::Risk::Modify, client), answer.callback());
    const QString identifier = requested.at(0).at(0).toString();
    // Another process can't answer instead of user
    QVERIFY(!manager.confirm(identifier, ConfirmationManager::Answer::Allow, u":1.4242"_s));
    QVERIFY(!manager.confirm(identifier, ConfirmationManager::Answer::Allow, {}));
    QCOMPARE(answer.value, -1);
    QVERIFY(manager.confirm(identifier, ConfirmationManager::Answer::Allow, client));
    QCOMPARE(answer.value, 1);
}

void ConfirmationManagerTest::shouldDenyAfterTimeout()
{
    ConfirmationManager manager;
    manager.setActionsAllowed(true);
    manager.setTimeout(200ms);
    Answer answer;
    manager.request(request(u"set_audio_mute"_s), answer.callback());
    QTRY_COMPARE_WITH_TIMEOUT(answer.value, 0, 2000);
    QVERIFY(!manager.hasPendingRequest());
}

void ConfirmationManagerTest::shouldDenyWithoutClient()
{
    ConfirmationManager manager;
    manager.setActionsAllowed(true);
    QSignalSpy requested(&manager, &ConfirmationManager::confirmationRequested);
    Answer answer;
    // Client which sent message is not on bus anymore: nobody can answer
    manager.request(request(u"set_audio_mute"_s, ConfirmationManager::Risk::Modify, u":1.4242"_s), answer.callback());
    QCOMPARE(answer.value, 0);
    QCOMPARE(requested.count(), 0);
}

void ConfirmationManagerTest::shouldAlwaysAllowModifyTools()
{
    ConfirmationManager manager;
    manager.setActionsAllowed(true);
    QSignalSpy requested(&manager, &ConfirmationManager::confirmationRequested);
    Answer first;
    manager.request(request(u"set_audio_mute"_s), first.callback());
    QVERIFY(manager.confirm(requested.at(0).at(0).toString(), ConfirmationManager::Answer::AlwaysAllow, {}));
    QCOMPARE(first.value, 1);
    QVERIFY(manager.isAlwaysAllowed(u"set_audio_mute"_s));

    Answer second;
    manager.request(request(u"set_audio_mute"_s), second.callback());
    QCOMPARE(second.value, 1);
    QCOMPARE(requested.count(), 1);
    // Other tools are still confirmed
    manager.request(request(u"set_audio_volume"_s), second.callback());
    QCOMPARE(requested.count(), 2);

    // Saved
    const ConfirmationManager other;
    QVERIFY(other.isAlwaysAllowed(u"set_audio_mute"_s));
}

void ConfirmationManagerTest::shouldNeverAlwaysAllowDestructiveTools()
{
    ConfirmationManager manager;
    manager.setActionsAllowed(true);
    QSignalSpy requested(&manager, &ConfirmationManager::confirmationRequested);
    Answer first;
    manager.request(request(u"restart_plasmashell"_s, ConfirmationManager::Risk::Destructive), first.callback());
    QCOMPARE(requested.at(0).at(5).toString(), u"destructive"_s);
    QVERIFY(manager.confirm(requested.at(0).at(0).toString(), ConfirmationManager::Answer::AlwaysAllow, {}));
    // Allowed once
    QCOMPARE(first.value, 1);
    QVERIFY(!manager.isAlwaysAllowed(u"restart_plasmashell"_s));

    // Even when configuration says so
    manager.setAlwaysAllowedTools({u"restart_plasmashell"_s});
    Answer second;
    manager.request(request(u"restart_plasmashell"_s, ConfirmationManager::Risk::Destructive), second.callback());
    QCOMPARE(second.value, -1);
    QCOMPARE(requested.count(), 2);
}

void ConfirmationManagerTest::shouldCreateSummary()
{
    QCOMPARE(ConfirmationManager::summary(u"set_default_audio_device"_s, {}, {{u"device"_s, u"headphones"_s}}), u"Use headphones as default output"_s);
    QCOMPARE(ConfirmationManager::summary(u"set_default_audio_device"_s, {}, {{u"device"_s, u"mic"_s}, {u"kind"_s, u"source"_s}}),
             u"Use mic as default input"_s);
    QCOMPARE(ConfirmationManager::summary(u"set_audio_volume"_s, {}, {{u"device"_s, u"speakers"_s}, {u"volume_percent"_s, 30}}),
             u"Set volume of speakers to 30%"_s);
    QCOMPARE(ConfirmationManager::summary(u"set_wifi_enabled"_s, {}, {{u"enabled"_s, false}}), u"Disable Wi-Fi"_s);
    // Small language models send numbers and booleans as strings: shown as server reads them
    QCOMPARE(ConfirmationManager::summary(u"set_audio_volume"_s, {}, {{u"volume_percent"_s, u"60"_s}}), u"Set volume of default device to 60%"_s);
    QCOMPARE(ConfirmationManager::summary(u"set_audio_mute"_s, {}, {{u"muted"_s, u"false"_s}}), u"Unmute default device"_s);
    // Invalid boolean: all arguments are shown
    QVERIFY(ConfirmationManager::summary(u"set_audio_mute"_s, u"Mute"_s, {{u"muted"_s, u"maybe"_s}}).contains("maybe"_L1));
    // Unknown tool shows all arguments: language model can't hide them
    QCOMPARE(ConfirmationManager::summary(u"unknown_tool"_s, u"Unknown"_s, {{u"key"_s, u"value"_s}}), u"Unknown with {\"key\":\"value\"}"_s);
}

#include "moc_confirmationmanagertest.cpp"
