/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scenariomodel.h"
#include <KLocalizedString>

using namespace Qt::Literals::StringLiterals;

// Prompts are translated: language model answers in language of user
ScenarioModel::ScenarioModel(QObject *parent)
    : QAbstractListModel{parent}
    , mScenarios({
          {
              .identifier = u"no_sound"_s,
              .title = i18nc("@action:button scenario", "No Sound"),
              .iconName = u"audio-volume-muted"_s,
              .prompt = i18n("I have no sound. Check my audio devices and tell me how to fix it."),
          },
          {
              .identifier = u"external_screen"_s,
              .title = i18nc("@action:button scenario", "External Screen"),
              .iconName = u"video-display"_s,
              .prompt = i18n("My external screen does not work correctly. Check my screens and tell me how to fix it."),
          },
          {
              .identifier = u"system_report"_s,
              .title = i18nc("@action:button scenario", "System Report"),
              .iconName = u"document-properties"_s,
              .prompt = i18n("Give me a short report about my system: versions, hardware and disk space."),
          },
      })
{
}

ScenarioModel::~ScenarioModel() = default;

int ScenarioModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return mScenarios.count();
}

QVariant ScenarioModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Scenario &scenario = mScenarios.at(index.row());
    switch (role) {
    case IdentifierRole:
        return scenario.identifier;
    case Qt::DisplayRole:
    case TitleRole:
        return scenario.title;
    case Qt::DecorationRole:
    case IconNameRole:
        return scenario.iconName;
    case PromptRole:
        return scenario.prompt;
    default:
        return {};
    }
}

QHash<int, QByteArray> ScenarioModel::roleNames() const
{
    return {
        {IdentifierRole, "identifier"_ba},
        {TitleRole, "title"_ba},
        {IconNameRole, "iconName"_ba},
        {PromptRole, "prompt"_ba},
    };
}

#include "moc_scenariomodel.cpp"
