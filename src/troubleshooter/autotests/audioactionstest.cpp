/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "audioactionstest.h"
#include "backends/audiobackend.h"
#include "core/privacyfilter.h"
#include "modules/audioactions.h"
#include <QTemporaryDir>
#include <QTest>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolTool>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(AudioActionsTest)

namespace
{
AudioProfile profile(const QString &name)
{
    AudioProfile result;
    result.name = name;
    return result;
}

// Audio server which applies changes immediately
class FakeAudioBackend : public AudioBackend
{
public:
    FakeAudioBackend()
    {
        AudioDevice speakers;
        speakers.name = u"speakers"_s;
        speakers.description = u"Speakers"_s;
        speakers.volumePercent = 40;
        speakers.isDefault = true;
        AudioDevice headphones;
        headphones.name = u"bluez_output.AA_BB_CC_DD_EE_FF.1"_s;
        headphones.description = u"Headphones"_s;
        headphones.volumePercent = 70;
        state.sinks = {speakers, headphones};
        AudioCard card;
        card.name = u"bluez_card.AA_BB_CC_DD_EE_FF"_s;
        card.description = u"Headphones"_s;
        card.profiles = {profile(u"a2dp-sink"_s), profile(u"headset-head-unit"_s)};
        card.activeProfile = u"a2dp-sink"_s;
        state.cards = {card};
        state.streams = {AudioStream{.index = 12, .application = u"Firefox"_s, .device = u"speakers"_s}};
    }

    void fetchState(QObject *, StateCallback callback) override
    {
        callback(state, {});
    }

    void setDefaultDevice(DeviceKind, const QString &name, QObject *, const ErrorCallback &callback) override
    {
        for (AudioDevice &device : state.sinks) {
            device.isDefault = device.name == name;
        }
        callback({});
    }

    void setVolume(DeviceKind, const QString &name, int volumePercent, QObject *, const ErrorCallback &callback) override
    {
        device(name)->volumePercent = volumePercent;
        callback({});
    }

    void setMuted(DeviceKind, const QString &name, bool muted, QObject *, const ErrorCallback &callback) override
    {
        device(name)->muted = muted;
        callback({});
    }

    void setCardProfile(const QString &, const QString &profile, QObject *, const ErrorCallback &callback) override
    {
        state.cards.first().activeProfile = profile;
        callback({});
    }

    void moveStream(quint32, const QString &sink, QObject *, const ErrorCallback &callback) override
    {
        state.streams.first().device = sink;
        callback({});
    }

    AudioDevice *device(const QString &name)
    {
        for (AudioDevice &device : state.sinks) {
            if (device.name == name) {
                return &device;
            }
        }
        return nullptr;
    }

    AudioState state;
};

struct Fixture {
    QTemporaryDir directory;
    ChangeJournal journal{directory.filePath(u"changes.json"_s)};
    ActionRunner runner{&journal};
    FakeAudioBackend backend;

    Fixture()
    {
        const auto actions = AudioActions::actions(&backend);
        for (const auto &[definition, action] : actions) {
            runner.addAction(definition.name(), action);
        }
    }

    // Returns error, or summary
    QString run(const QString &tool, const QJsonObject &arguments)
    {
        QString answer;
        QObject context;
        bool finished = false;
        runner.run(tool, arguments, &context, [&answer, &finished](const QJsonObject &result, const QString &error) {
            answer = error.isEmpty() ? result.value("summary"_L1).toString() : u"error: "_s + error;
            finished = true;
        });
        (void)QTest::qWaitFor(
            [&finished]() {
                return finished;
            },
            5000);
        return answer;
    }
};
}

AudioActionsTest::AudioActionsTest(QObject *parent)
    : QObject{parent}
{
}

void AudioActionsTest::shouldSetDefaultDevice()
{
    Fixture fixture;
    QCOMPARE(fixture.run(u"set_default_audio_device"_s, {{u"device"_s, u"bluez_output.AA_BB_CC_DD_EE_FF.1"_s}}),
             u"Default audio device: Speakers → Headphones"_s);
    QVERIFY(fixture.backend.device(u"bluez_output.AA_BB_CC_DD_EE_FF.1"_s)->isDefault);
}

void AudioActionsTest::shouldSetVolumeOfDefaultDevice()
{
    Fixture fixture;
    QCOMPARE(fixture.run(u"set_audio_volume"_s, {{u"volume_percent"_s, 80}}), u"Volume of Speakers: 40% → 80%"_s);
    QCOMPARE(fixture.backend.device(u"speakers"_s)->volumePercent, 80);
    QVERIFY(fixture.run(u"set_audio_volume"_s, {{u"volume_percent"_s, 150}}).startsWith("error"_L1));
    // Small language models send numbers and booleans as strings
    QCOMPARE(fixture.run(u"set_audio_volume"_s, {{u"volume_percent"_s, u"60"_s}}), u"Volume of Speakers: 80% → 60%"_s);
    QVERIFY(fixture.run(u"set_audio_volume"_s, {{u"volume_percent"_s, u"loud"_s}}).startsWith("error"_L1));
    QCOMPARE(fixture.run(u"set_audio_mute"_s, {{u"muted"_s, u"true"_s}}), u"Speakers muted"_s);
    QVERIFY(fixture.backend.device(u"speakers"_s)->muted);
}

void AudioActionsTest::shouldRejectUnknownDevice()
{
    Fixture fixture;
    QVERIFY(fixture.run(u"set_default_audio_device"_s, {{u"device"_s, u"hdmi"_s}}).contains("Unknown audio device"_L1));
    QVERIFY(fixture.run(u"set_audio_mute"_s, {{u"device"_s, u"hdmi"_s}, {u"muted"_s, true}}).contains("Unknown audio device"_L1));
    QVERIFY(fixture.journal.changes().isEmpty());
}

void AudioActionsTest::shouldChangeCardProfile()
{
    Fixture fixture;
    const QJsonObject arguments{{u"card"_s, u"bluez_card.AA_BB_CC_DD_EE_FF"_s}, {u"profile"_s, u"headset-head-unit"_s}};
    QCOMPARE(fixture.run(u"set_card_profile"_s, arguments), u"Profile of Headphones: a2dp-sink → headset-head-unit"_s);
    QVERIFY(fixture.run(u"set_card_profile"_s, {{u"card"_s, u"bluez_card.AA_BB_CC_DD_EE_FF"_s}, {u"profile"_s, u"hdmi"_s}}).contains("Unknown profile"_L1));
}

void AudioActionsTest::shouldMoveStream()
{
    Fixture fixture;
    QCOMPARE(fixture.run(u"move_audio_stream"_s, {{u"stream"_s, 12}, {u"device"_s, u"bluez_output.AA_BB_CC_DD_EE_FF.1"_s}}),
             u"Sound of Firefox: Speakers → Headphones"_s);
    QVERIFY(fixture.run(u"move_audio_stream"_s, {{u"stream"_s, 99}, {u"device"_s, u"speakers"_s}}).contains("Unknown audio stream"_L1));
}

void AudioActionsTest::shouldRestorePseudonyms()
{
    // Language model only sees name of Bluetooth device with a pseudonym of its address
    PrivacyFilter filter;
    const QString pseudonymized = filter.filter(u"bluez_output.AA_BB_CC_DD_EE_FF.1"_s);
    QVERIFY(!pseudonymized.contains("AA_BB"_L1));
    QCOMPARE(filter.restore(QJsonValue(pseudonymized)).toString(), u"bluez_output.AA:BB:CC:DD:EE:FF.1"_s);
}

#include "moc_audioactionstest.cpp"
