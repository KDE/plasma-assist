/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "changejournal.h"
#include "plasmaassist_troubleshooter_debug.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

using namespace Qt::Literals::StringLiterals;

namespace
{
QString defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + u"/plasma-assist/changes.json"_s;
}
}

QJsonObject ChangeJournal::Change::toJson() const
{
    return QJsonObject{
        {u"id"_s, identifier},
        {u"dateTime"_s, dateTime.toString(Qt::ISODate)},
        {u"tool"_s, tool},
        {u"summary"_s, summary},
        {u"arguments"_s, arguments},
        {u"before"_s, before},
        {u"after"_s, after},
        {u"undoable"_s, undoable},
        {u"undone"_s, undone},
    };
}

ChangeJournal::Change ChangeJournal::Change::fromJson(const QJsonObject &object)
{
    return Change{
        .identifier = object.value("id"_L1).toString(),
        .dateTime = QDateTime::fromString(object.value("dateTime"_L1).toString(), Qt::ISODate),
        .tool = object.value("tool"_L1).toString(),
        .summary = object.value("summary"_L1).toString(),
        .arguments = object.value("arguments"_L1).toObject(),
        .before = object.value("before"_L1).toObject(),
        .after = object.value("after"_L1).toObject(),
        .undoable = object.value("undoable"_L1).toBool(),
        .undone = object.value("undone"_L1).toBool(),
    };
}

ChangeJournal::ChangeJournal(const QString &filePath)
    : mFilePath(filePath.isEmpty() ? defaultFilePath() : filePath)
{
    load();
}

ChangeJournal::~ChangeJournal() = default;

QString ChangeJournal::filePath() const
{
    return mFilePath;
}

QList<ChangeJournal::Change> ChangeJournal::changes() const
{
    load();
    return mChanges;
}

std::optional<ChangeJournal::Change> ChangeJournal::change(const QString &identifier) const
{
    load();
    for (const Change &change : mChanges) {
        if (change.identifier == identifier) {
            return change;
        }
    }
    return std::nullopt;
}

ChangeJournal::Change ChangeJournal::add(const QString &tool,
                                         const QString &summary,
                                         const QJsonObject &arguments,
                                         const QJsonObject &before,
                                         const QJsonObject &after,
                                         bool undoable)
{
    const Change change{
        .identifier = QUuid::createUuid().toString(QUuid::Id128).left(8),
        .dateTime = QDateTime::currentDateTime(),
        .tool = tool,
        .summary = summary,
        .arguments = arguments,
        .before = before,
        .after = after,
        .undoable = undoable,
        .undone = false,
    };
    load();
    mChanges.prepend(change);
    if (mChanges.count() > maximumChanges) {
        mChanges.resize(maximumChanges);
    }
    save();
    return change;
}

void ChangeJournal::setUndone(const QString &identifier)
{
    load();
    for (Change &change : mChanges) {
        if (change.identifier == identifier) {
            change.undone = true;
            save();
            return;
        }
    }
}

void ChangeJournal::load() const
{
    mChanges.clear();
    QFile file(mFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonArray changes = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &value : changes) {
        if (const Change change = Change::fromJson(value.toObject()); !change.identifier.isEmpty()) {
            mChanges.append(change);
        }
    }
}

void ChangeJournal::save() const
{
    QDir().mkpath(QFileInfo(mFilePath).absolutePath());
    QSaveFile file(mFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(PLASMA_ASSIST_TROUBLESHOOTER_LOG) << "Impossible to save changes:" << file.errorString();
        return;
    }
    QJsonArray changes;
    for (const Change &change : mChanges) {
        changes.append(change.toJson());
    }
    file.write(QJsonDocument(changes).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(PLASMA_ASSIST_TROUBLESHOOTER_LOG) << "Impossible to save changes:" << file.errorString();
    }
}
