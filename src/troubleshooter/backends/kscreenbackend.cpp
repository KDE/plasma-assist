/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kscreenbackend.h"
#include <KScreen/Config>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/Output>
#include <KScreen/SetConfigOperation>

using namespace Qt::Literals::StringLiterals;

namespace
{
QString rotation(KScreen::Output::Rotation rotation)
{
    switch (rotation) {
    case KScreen::Output::None:
        return u"none"_s;
    case KScreen::Output::Left:
        return u"left"_s;
    case KScreen::Output::Inverted:
        return u"inverted"_s;
    case KScreen::Output::Right:
        return u"right"_s;
    case KScreen::Output::Flipped:
    case KScreen::Output::Flipped90:
    case KScreen::Output::Flipped180:
    case KScreen::Output::Flipped270:
        break;
    }
    return u"flipped"_s;
}

DisplayOutput displayOutput(const KScreen::OutputPtr &output)
{
    DisplayOutput result{
        .id = output->id(),
        .name = output->name(),
        .type = output->typeName(),
        .vendor = output->vendor(),
        .model = output->model(),
        .modes = {},
        .currentModeId = output->currentModeId(),
        .preferredModeId = output->preferredModeId(),
        .position = output->pos(),
        .rotation = rotation(output->rotation()),
        .scale = output->scale(),
        .priority = output->priority(),
        .connected = output->isConnected(),
        .enabled = output->isEnabled(),
    };
    const KScreen::ModeList modes = output->modes();
    for (const KScreen::ModePtr &mode : modes) {
        result.modes.append(DisplayMode{.id = mode->id(), .size = mode->size(), .refreshRate = mode->refreshRate()});
    }
    return result;
}
}

KScreenBackend::KScreenBackend() = default;

KScreenBackend::~KScreenBackend() = default;

void KScreenBackend::fetchOutputs(QObject *context, OutputsCallback callback)
{
    // Operation deletes itself when finished
    auto operation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID);
    QObject::connect(operation, &KScreen::ConfigOperation::finished, context, [callback](KScreen::ConfigOperation *operation) {
        if (operation->hasError()) {
            callback({}, u"Impossible to get display configuration: %1"_s.arg(operation->errorString()));
            return;
        }
        QList<DisplayOutput> outputs;
        const KScreen::OutputList kscreenOutputs = operation->config()->outputs();
        for (const KScreen::OutputPtr &output : kscreenOutputs) {
            outputs.append(displayOutput(output));
        }
        callback(outputs, {});
    });
}

void KScreenBackend::applyOutputSettings(const QString &name, const DisplayOutputSettings &settings, QObject *context, ErrorCallback callback)
{
    // Configuration is read again: it can have changed since state was read
    auto operation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID);
    QObject::connect(operation, &KScreen::ConfigOperation::finished, context, [name, settings, context, callback](KScreen::ConfigOperation *operation) {
        if (operation->hasError()) {
            callback(u"Impossible to get display configuration: %1"_s.arg(operation->errorString()));
            return;
        }
        const KScreen::ConfigPtr config = operation->config();
        const KScreen::OutputList outputs = config->outputs();
        const auto it = std::ranges::find_if(outputs, [&name](const KScreen::OutputPtr &output) {
            return output->name() == name;
        });
        if (it == outputs.cend() || !(*it)->isConnected()) {
            callback(u"Unknown or disconnected output: %1"_s.arg(name));
            return;
        }
        const KScreen::OutputPtr output = *it;
        output->setEnabled(settings.enabled);
        if (settings.enabled) {
            if (!settings.modeId.isEmpty()) {
                output->setCurrentModeId(settings.modeId);
            }
            output->setScale(settings.scale);
            output->setPos(settings.position);
        }
        if (!KScreen::Config::canBeApplied(config, KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen)) {
            callback(u"Display configuration can't be applied"_s);
            return;
        }
        auto setOperation = new KScreen::SetConfigOperation(config);
        QObject::connect(setOperation, &KScreen::ConfigOperation::finished, context, [callback](KScreen::ConfigOperation *operation) {
            callback(operation->hasError() ? u"Impossible to apply display configuration: %1"_s.arg(operation->errorString()) : QString());
        });
    });
}
