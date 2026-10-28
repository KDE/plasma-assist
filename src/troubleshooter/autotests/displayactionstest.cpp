/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "displayactionstest.h"
#include "backends/displaybackend.h"
#include "modules/displayactions.h"
#include <QTemporaryDir>
#include <QTest>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolTool>

using namespace Qt::Literals::StringLiterals;
QTEST_GUILESS_MAIN(DisplayActionsTest)

namespace
{
// Compositor which applies changes immediately
class FakeDisplayBackend : public DisplayBackend
{
public:
    FakeDisplayBackend()
    {
        DisplayOutput panel;
        panel.id = 1;
        panel.name = u"eDP-1"_s;
        panel.modes = {DisplayMode{.id = u"1"_s, .size = QSize(2880, 1800), .refreshRate = 120}};
        panel.currentModeId = u"1"_s;
        panel.preferredModeId = u"1"_s;
        panel.rotation = u"none"_s;
        panel.scale = 1.5;
        panel.connected = true;
        panel.enabled = true;
        DisplayOutput hdmi;
        hdmi.id = 2;
        hdmi.name = u"HDMI-A-1"_s;
        hdmi.modes = {DisplayMode{.id = u"10"_s, .size = QSize(3840, 2160), .refreshRate = 60},
                      DisplayMode{.id = u"11"_s, .size = QSize(3840, 2160), .refreshRate = 30},
                      DisplayMode{.id = u"12"_s, .size = QSize(1920, 1080), .refreshRate = 59.94},
                      DisplayMode{.id = u"13"_s, .size = QSize(1920, 1080), .refreshRate = 50}};
        hdmi.preferredModeId = u"10"_s;
        hdmi.rotation = u"none"_s;
        hdmi.connected = true;
        DisplayOutput displayPort;
        displayPort.id = 3;
        displayPort.name = u"DP-1"_s;
        outputs = {panel, hdmi, displayPort};
    }

    void fetchOutputs(QObject *, OutputsCallback callback) override
    {
        callback(outputs, {});
    }

    void applyOutputSettings(const QString &name, const DisplayOutputSettings &settings, QObject *, ErrorCallback callback) override
    {
        DisplayOutput *output = this->output(name);
        output->enabled = settings.enabled;
        if (settings.enabled) {
            output->currentModeId = settings.modeId;
            output->scale = settings.scale;
            output->position = settings.position;
        }
        callback({});
    }

    DisplayOutput *output(const QString &name)
    {
        for (DisplayOutput &output : outputs) {
            if (output.name == name) {
                return &output;
            }
        }
        return nullptr;
    }

    QList<DisplayOutput> outputs;
};

struct Fixture {
    QTemporaryDir directory;
    ChangeJournal journal{directory.filePath(u"changes.json"_s)};
    ActionRunner runner{&journal};
    FakeDisplayBackend backend;

    Fixture()
    {
        const auto actions = DisplayActions::actions(&backend);
        for (const auto &[definition, action] : actions) {
            runner.addAction(definition.name(), action);
        }
    }

    // Returns error, or summary
    QString run(const QJsonObject &arguments)
    {
        QString answer;
        QObject context;
        bool finished = false;
        runner.run(u"set_output_config"_s, arguments, &context, [&answer, &finished](const QJsonObject &result, const QString &error) {
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

DisplayActionsTest::DisplayActionsTest(QObject *parent)
    : QObject{parent}
{
}

void DisplayActionsTest::shouldEnableOutputAtRightOfOthers()
{
    Fixture fixture;
    // Native mode, at the right of panel (2880 / 1.5 = 1920 logical pixels)
    QCOMPARE(fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"enabled"_s, true}}), u"Display HDMI-A-1: disabled → 3840x2160@60Hz, scale 100%, position 1920,0"_s);
    const DisplayOutput hdmi = *fixture.backend.output(u"HDMI-A-1"_s);
    QVERIFY(hdmi.enabled);
    QCOMPARE(hdmi.currentModeId, u"10"_s);
    QCOMPARE(hdmi.position, QPoint(1920, 0));
    // Small language models send booleans as strings
    QCOMPARE(fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"enabled"_s, u"false"_s}}),
             u"Display HDMI-A-1: 3840x2160@60Hz, scale 100%, position 1920,0 → disabled"_s);
    QVERIFY(!fixture.backend.output(u"HDMI-A-1"_s)->enabled);
}

void DisplayActionsTest::shouldChangeResolutionAndRefreshRate()
{
    Fixture fixture;
    // Setting a resolution enables output, highest refresh rate is used
    (void)fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"resolution"_s, u"1920x1080"_s}});
    QCOMPARE(fixture.backend.output(u"HDMI-A-1"_s)->currentModeId, u"12"_s);
    // Rounded refresh rate of list_outputs
    (void)fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"refresh_rate"_s, 50}});
    QCOMPARE(fixture.backend.output(u"HDMI-A-1"_s)->currentModeId, u"13"_s);
    (void)fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"resolution"_s, u"3840 x 2160"_s}, {u"refresh_rate"_s, u"30"_s}});
    QCOMPARE(fixture.backend.output(u"HDMI-A-1"_s)->currentModeId, u"11"_s);
}

void DisplayActionsTest::shouldChangeScaleAndPosition()
{
    Fixture fixture;
    QCOMPARE(fixture.run({{u"output"_s, u"eDP-1"_s}, {u"scale"_s, 1.25}}),
             u"Display eDP-1: 2880x1800@120Hz, scale 150%, position 0,0 → 2880x1800@120Hz, scale 125%, position 0,0"_s);
    QCOMPARE(fixture.backend.output(u"eDP-1"_s)->scale, 1.25);
    // Multiples of 5%
    (void)fixture.run({{u"output"_s, u"eDP-1"_s}, {u"scale"_s, u"1.33"_s}});
    QCOMPARE(fixture.backend.output(u"eDP-1"_s)->scale, 1.35);
    (void)fixture.run({{u"output"_s, u"eDP-1"_s}, {u"x"_s, -1920}});
    QCOMPARE(fixture.backend.output(u"eDP-1"_s)->position, QPoint(-1920, 0));
    QVERIFY(fixture.run({{u"output"_s, u"eDP-1"_s}, {u"x"_s, -1920}}).startsWith("Nothing to change"_L1));
}

void DisplayActionsTest::shouldRejectInvalidArguments()
{
    Fixture fixture;
    QVERIFY(fixture.run({{u"output"_s, u"VGA-1"_s}, {u"enabled"_s, true}}).contains("Unknown or disconnected output"_L1));
    QVERIFY(fixture.run({{u"output"_s, u"DP-1"_s}, {u"enabled"_s, true}}).contains("Unknown or disconnected output"_L1));
    QVERIFY(fixture.run({{u"output"_s, u"HDMI-A-1"_s}}).contains("at least one"_L1));
    QVERIFY(fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"resolution"_s, u"1280x720"_s}}).contains("available: 3840x2160, 1920x1080"_L1));
    QVERIFY(fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"resolution"_s, u"big"_s}}).contains("like 1920x1080"_L1));
    QVERIFY(fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"refresh_rate"_s, 144}}).contains("no refresh rate 144"_L1));
    QVERIFY(fixture.run({{u"output"_s, u"eDP-1"_s}, {u"scale"_s, 5}}).contains("scale must be between"_L1));
    QVERIFY(fixture.journal.changes().isEmpty());
}

void DisplayActionsTest::shouldRefuseToDisableLastOutput()
{
    Fixture fixture;
    QVERIFY(fixture.run({{u"output"_s, u"eDP-1"_s}, {u"enabled"_s, false}}).contains("only enabled output"_L1));
    QVERIFY(fixture.backend.output(u"eDP-1"_s)->enabled);

    (void)fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"enabled"_s, true}});
    const QString summary = fixture.run({{u"output"_s, u"eDP-1"_s}, {u"enabled"_s, false}});
    QVERIFY2(summary.endsWith(u"→ disabled"_s), qPrintable(summary));
}

void DisplayActionsTest::shouldUndoChange()
{
    Fixture fixture;
    (void)fixture.run({{u"output"_s, u"HDMI-A-1"_s}, {u"enabled"_s, true}});
    const QString identifier = fixture.journal.changes().constFirst().identifier;
    bool finished = false;
    QString undoError;
    QObject context;
    fixture.runner.undo(identifier, &context, [&finished, &undoError](const QJsonObject &, const QString &error) noexcept {
        undoError = error;
        finished = true;
    });
    QVERIFY(QTest::qWaitFor([&finished]() {
        return finished;
    }));
    QVERIFY2(undoError.isEmpty(), qPrintable(undoError));
    QVERIFY(!fixture.backend.output(u"HDMI-A-1"_s)->enabled);
}

#include "moc_displayactionstest.cpp"
