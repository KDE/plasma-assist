/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QAbstractListModel>

// Common problems shown on home page. Selecting one sends its prompt to assistant.
// TODO (M7): use prompts of troubleshooter server (ListScenarios)
class ScenarioModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdentifierRole = Qt::UserRole + 1,
        TitleRole,
        IconNameRole,
        PromptRole,
    };
    Q_ENUM(Roles)

    explicit ScenarioModel(QObject *parent = nullptr);
    ~ScenarioModel() override;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    struct Scenario {
        QString identifier;
        QString title;
        QString iconName;
        QString prompt;
    };
    const QList<Scenario> mScenarios;
};
