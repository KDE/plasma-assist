/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

// Changes made by actions, saved in a file to undo them later (also after a restart).
// File is read again before each access: several servers can use it (one by client)
class ChangeJournal
{
public:
    struct Change {
        QString identifier;
        QDateTime dateTime;
        QString tool;
        // Translated, e.g. "Default output: HDMI → Headphones"
        QString summary;
        QJsonObject arguments;
        QJsonObject before;
        QJsonObject after;
        bool undoable = true;
        bool undone = false;
        [[nodiscard]] QJsonObject toJson() const;
        [[nodiscard]] static Change fromJson(const QJsonObject &object);
    };

    // Number of changes kept
    static constexpr int maximumChanges = 50;

    // \a filePath: empty for default location (~/.local/state/plasma-assist/changes.json)
    explicit ChangeJournal(const QString &filePath = {});
    ~ChangeJournal();

    [[nodiscard]] QString filePath() const;
    // Most recent first
    [[nodiscard]] QList<Change> changes() const;
    [[nodiscard]] std::optional<Change> change(const QString &identifier) const;

    Change add(const QString &tool, const QString &summary, const QJsonObject &arguments, const QJsonObject &before, const QJsonObject &after, bool undoable);
    void setUndone(const QString &identifier);

private:
    void load() const;
    void save() const;

    const QString mFilePath;
    mutable QList<Change> mChanges;
};
