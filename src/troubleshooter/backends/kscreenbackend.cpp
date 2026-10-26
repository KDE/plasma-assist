/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kscreenbackend.h"
#include <KScreen/Config>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/Output>

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
