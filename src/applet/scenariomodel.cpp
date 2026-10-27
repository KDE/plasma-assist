/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scenariomodel.h"
#include <KLocalizedString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::Literals::StringLiterals;

ScenarioModel::ScenarioModel(QObject *parent)
    : QAbstractListModel{parent}
    , mScenarios({
          {
              .identifier = u"no_sound"_s,
              .title = i18nc("@action:button scenario", "No Sound"),
              .iconName = u"audio-volume-muted"_s,
              .description = {},
          },
          {
              .identifier = u"external_display"_s,
              .title = i18nc("@action:button scenario", "External Screen"),
              .iconName = u"video-display"_s,
              .description = {},
          },
          {
              .identifier = u"network_problem"_s,
              .title = i18nc("@action:button scenario", "Internet or Wi-Fi"),
              .iconName = u"network-wireless"_s,
              .description = {},
          },
          {
              .identifier = u"no_sleep"_s,
              .title = i18nc("@action:button scenario", "Computer Does Not Sleep"),
              .iconName = u"system-suspend"_s,
              .description = {},
          },
          {
              .identifier = u"app_crash"_s,
              .title = i18nc("@action:button scenario", "Application Crashes"),
              .iconName = u"tools-report-bug"_s,
              .description = {},
          },
          {
              .identifier = u"system_report"_s,
              .title = i18nc("@action:button scenario", "System Report"),
              .iconName = u"document-properties"_s,
              .description = {},
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
    case DescriptionRole:
        return scenario.description;
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
        {DescriptionRole, "description"_ba},
    };
}

void ScenarioModel::setScenarios(const QString &json)
{
    QList<Scenario> scenarios;
    const QJsonArray array = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue &value : array) {
        const QJsonObject scenario = value.toObject();
        if (const QString name = scenario.value("name"_L1).toString(); !name.isEmpty()) {
            const QString title = scenario.value("title"_L1).toString();
            const QString icon = scenario.value("icon"_L1).toString();
            scenarios.append({
                .identifier = name,
                .title = title.isEmpty() ? name : title,
                .iconName = icon.isEmpty() ? u"help-browser"_s : icon,
                .description = scenario.value("description"_L1).toString(),
            });
        }
    }
    // Keep built-in list when server has no scenario
    if (scenarios.isEmpty()) {
        return;
    }
    beginResetModel();
    mScenarios = scenarios;
    endResetModel();
}

QString ScenarioModel::title(const QString &identifier) const
{
    for (const Scenario &scenario : mScenarios) {
        if (scenario.identifier == identifier) {
            return scenario.title;
        }
    }
    return identifier;
}

#include "moc_scenariomodel.cpp"
