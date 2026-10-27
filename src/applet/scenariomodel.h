/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QAbstractListModel>

// Common problems shown on home page: guided scenarios of troubleshooter server (ListScenarios).
// Built-in list is used until daemon answers.
class ScenarioModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdentifierRole = Qt::UserRole + 1,
        TitleRole,
        IconNameRole,
        DescriptionRole,
    };
    Q_ENUM(Roles)

    explicit ScenarioModel(QObject *parent = nullptr);
    ~ScenarioModel() override;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // JSON array returned by ListScenarios()
    void setScenarios(const QString &json);
    [[nodiscard]] QString title(const QString &identifier) const;

private:
    struct Scenario {
        QString identifier;
        QString title;
        QString iconName;
        QString description;
    };
    QList<Scenario> mScenarios;
};
