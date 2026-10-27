/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

// Masks personal data in results sent to language model (which can be a cloud provider):
// MAC and IP addresses, user name, home directory, host name and words given by providers (Wi-Fi network names…).
// Each value is replaced by a stable pseudonym ("<mac-1>"), so language model can still tell values apart.
class PrivacyFilter
{
public:
    // Returns words to mask, e.g. SSIDs of Wi-Fi networks
    using WordsProvider = std::function<QStringList()>;

    PrivacyFilter();
    ~PrivacyFilter();

    // Filter used for tool results
    [[nodiscard]] static PrivacyFilter *instance();

    // Enabled by default, can be disabled in plasma-assistrc ([Privacy] FilterPersonalData=false)
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool enabled);

    void setUserName(const QString &userName);
    void setHomePath(const QString &homePath);
    void setHostName(const QString &hostName);
    void addWordsProvider(const WordsProvider &provider);

    [[nodiscard]] QString filter(const QString &text);
    // Filters strings and keys of objects
    [[nodiscard]] QJsonValue filter(const QJsonValue &value);

    // Original value of \a pseudonym (empty if unknown): used when language model gives a pseudonym as argument
    [[nodiscard]] QString originalValue(const QString &pseudonym) const;
    // Replaces known pseudonyms in strings of \a value by original values (arguments of actions)
    [[nodiscard]] QJsonValue restore(const QJsonValue &value) const;

private:
    [[nodiscard]] QString pseudonym(const QString &category, const QString &value);
    [[nodiscard]] QStringList sensitiveWords() const;

    QList<WordsProvider> mWordsProviders;
    // category + value → pseudonym, and reverse
    QHash<QString, QString> mPseudonyms;
    QHash<QString, QString> mOriginalValues;
    QHash<QString, int> mCounters;
    QString mUserName;
    QString mHomePath;
    QString mHostName;
    bool mEnabled = true;
};
