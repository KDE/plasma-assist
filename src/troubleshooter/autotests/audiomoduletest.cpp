/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "audiomoduletest.h"
#include "modules/audiomodule.h"
#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(AudioModuleTest)

namespace
{
AudioPort port(const QString &name, const QString &type, const QString &availability)
{
    return AudioPort{.name = name, .description = name, .type = type, .availability = availability};
}

AudioDevice speakers()
{
    AudioDevice device;
    device.name = u"alsa_output.pci-0000_00_1f.3.analog-stereo"_s;
    device.description = u"Built-in Audio"_s;
    device.state = u"running"_s;
    device.ports = {port(u"analog-output-speaker"_s, u"Speaker"_s, u"available"_s)};
    device.activePort = u"analog-output-speaker"_s;
    device.volumePercent = 50;
    device.isDefault = true;
    return device;
}

AudioDevice hdmi()
{
    AudioDevice device;
    device.name = u"alsa_output.pci-0000_01_00.1.hdmi-stereo"_s;
    device.description = u"HDMI Audio"_s;
    device.ports = {port(u"hdmi-output-0"_s, u"HDMI"_s, u"available"_s)};
    device.activePort = u"hdmi-output-0"_s;
    device.volumePercent = 100;
    return device;
}

AudioDevice usbHeadset()
{
    AudioDevice device;
    device.name = u"alsa_output.usb-headset.analog-stereo"_s;
    device.description = u"USB Headset"_s;
    device.formFactor = u"headset"_s;
    device.volumePercent = 70;
    return device;
}
}

AudioModuleTest::AudioModuleTest(QObject *parent)
    : QObject{parent}
{
}

void AudioModuleTest::shouldHaveNoHintWhenEverythingIsFine()
{
    AudioState state;
    state.sinks = {speakers(), hdmi()};
    QVERIFY(AudioModule::hints(state).isEmpty());
}

void AudioModuleTest::shouldDetectNoOutput()
{
    const QStringList hints = AudioModule::hints(AudioState{});
    QCOMPARE(hints.count(), 1);
    QVERIFY(hints.first().startsWith("No output device found"_L1));
}

void AudioModuleTest::shouldDetectMutedDefaultOutput()
{
    AudioDevice device = speakers();
    device.muted = true;
    device.volumePercent = 0;
    AudioState state;
    state.sinks = {device};
    const QStringList hints = AudioModule::hints(state);
    QCOMPARE(hints, (QStringList{u"Default output \"Built-in Audio\" is muted"_s, u"Default output \"Built-in Audio\" volume is 0%"_s}));
}

void AudioModuleTest::shouldDetectHeadphonesOnAnotherOutput()
{
    // Headset plugged, default output is HDMI
    AudioDevice defaultHdmi = hdmi();
    defaultHdmi.isDefault = true;
    AudioDevice notDefaultSpeakers = speakers();
    notDefaultSpeakers.isDefault = false;
    AudioState state;
    state.sinks = {notDefaultSpeakers, defaultHdmi, usbHeadset()};
    const QStringList hints = AudioModule::hints(state);
    QCOMPARE(hints, QStringList{u"Headphones are available on output \"USB Headset\" but default output is \"HDMI Audio\""_s});
}

void AudioModuleTest::shouldDetectHeadphonesOnInactivePort()
{
    AudioDevice device = speakers();
    device.ports.append(port(u"analog-output-headphones"_s, u"Headphones"_s, u"available"_s));
    AudioState state;
    state.sinks = {device};
    const QStringList hints = AudioModule::hints(state);
    QCOMPARE(hints.count(), 1);
    QVERIFY(hints.first().startsWith("Headphones are plugged on port \"analog-output-headphones\""_L1));
}

void AudioModuleTest::shouldDetectUnpluggedActivePort()
{
    AudioDevice device = speakers();
    device.ports = {port(u"analog-output-headphones"_s, u"Headphones"_s, u"unavailable"_s)};
    device.activePort = u"analog-output-headphones"_s;
    AudioState state;
    state.sinks = {device};
    QCOMPARE(AudioModule::hints(state), QStringList{u"Active port \"analog-output-headphones\" of default output \"Built-in Audio\" is unplugged"_s});
}

void AudioModuleTest::shouldDetectMutedStream()
{
    AudioState state;
    state.sinks = {speakers()};
    state.streams = {AudioStream{.index = 3, .application = u"Firefox"_s, .device = speakers().name, .volumePercent = 100, .muted = true, .corked = false}};
    QCOMPARE(AudioModule::hints(state), QStringList{u"Sound of application \"Firefox\" is muted"_s});
}

void AudioModuleTest::shouldFilterDevicesByKind()
{
    AudioState state;
    state.serverName = u"PipeWire"_s;
    state.sinks = {speakers()};
    AudioDevice microphone;
    microphone.name = u"mic"_s;
    state.sources = {microphone};

    const QJsonObject sinks = AudioModule::devicesToJson(state, u"sink"_s);
    QVERIFY(sinks.contains("sinks"_L1));
    QVERIFY(!sinks.contains("sources"_L1));
    QCOMPARE(sinks.value("server"_L1).toObject().value("name"_L1).toString(), u"PipeWire"_s);
    const QJsonObject sink = sinks.value("sinks"_L1).toArray().first().toObject();
    QCOMPARE(sink.value("default"_L1).toBool(), true);
    QCOMPARE(sink.value("volumePercent"_L1).toInt(), 50);
    QCOMPARE(sink.value("ports"_L1).toArray().first().toObject().value("type"_L1).toString(), u"Speaker"_s);

    const QJsonObject sources = AudioModule::devicesToJson(state, u"source"_s);
    QVERIFY(!sources.contains("sinks"_L1));
    QVERIFY(sources.contains("sources"_L1));

    const QJsonObject all = AudioModule::devicesToJson(state, u"all"_s);
    QVERIFY(all.contains("sinks"_L1));
    QVERIFY(all.contains("sources"_L1));
}

void AudioModuleTest::shouldConvertCards()
{
    AudioState state;
    state.cards = {AudioCard{
        .index = 42,
        .name = u"bluez_card.headset"_s,
        .description = u"Headset"_s,
        .profiles = {AudioProfile{.name = u"a2dp-sink"_s,
                                  .description = u"High Fidelity Playback (A2DP Sink)"_s,
                                  .availability = u"available"_s,
                                  .sinkCount = 1,
                                  .sourceCount = 0},
                     AudioProfile{.name = u"headset-head-unit"_s,
                                  .description = u"Headset Head Unit (HSP/HFP)"_s,
                                  .availability = u"available"_s,
                                  .sinkCount = 1,
                                  .sourceCount = 1}},
        .activeProfile = u"headset-head-unit"_s,
        .ports = {},
    }};
    const QJsonArray cards = AudioModule::cardsToJson(state).value("cards"_L1).toArray();
    QCOMPARE(cards.count(), 1);
    const QJsonObject card = cards.first().toObject();
    QCOMPARE(card.value("index"_L1).toInt(), 42);
    QCOMPARE(card.value("activeProfile"_L1).toString(), u"headset-head-unit"_s);
    QCOMPARE(card.value("profiles"_L1).toArray().count(), 2);
    QCOMPARE(card.value("profiles"_L1).toArray().first().toObject().value("sinkCount"_L1).toInt(), 1);
}

void AudioModuleTest::shouldConvertStreams()
{
    AudioState state;
    state.streams = {AudioStream{.index = 7, .application = u"Elisa"_s, .device = u"hdmi"_s, .volumePercent = 80, .muted = false, .corked = true}};
    const QJsonObject stream = AudioModule::streamsToJson(state).value("streams"_L1).toArray().first().toObject();
    QCOMPARE(stream.value("application"_L1).toString(), u"Elisa"_s);
    QCOMPARE(stream.value("device"_L1).toString(), u"hdmi"_s);
    QCOMPARE(stream.value("paused"_L1).toBool(), true);
}

#include "moc_audiomoduletest.cpp"
