/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "privacyfilter.h"
#include <QDir>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSysInfo>

using namespace Qt::Literals::StringLiterals;

namespace
{
// Too short values would mask common words
constexpr qsizetype minimumWordLength = 3;

// Replaces each match of \a expression in \a text by result of \a replacement (match is kept when it returns null string)
QString replaceMatches(const QString &text, const QRegularExpression &expression, const std::function<QString(const QString &match)> &replacement)
{
    QString result;
    qsizetype last = 0;
    QRegularExpressionMatchIterator it = expression.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString value = replacement(match.captured());
        if (value.isNull()) {
            continue;
        }
        result += QStringView(text).mid(last, match.capturedStart() - last);
        result += value;
        last = match.capturedEnd();
    }
    if (last == 0) {
        return text;
    }
    result += QStringView(text).mid(last);
    return result;
}

QRegularExpression wordExpression(const QString &word)
{
    // Whole word only: user "max" must not change "maximum"
    return QRegularExpression(u"(?<![\\w])%1(?![\\w])"_s.arg(QRegularExpression::escape(word)));
}
}

PrivacyFilter::PrivacyFilter()
    : mUserName(QDir::home().dirName())
    , mHomePath(QDir::homePath())
    , mHostName(QSysInfo::machineHostName())
{
}

PrivacyFilter::~PrivacyFilter() = default;

PrivacyFilter *PrivacyFilter::instance()
{
    static PrivacyFilter filter;
    return &filter;
}

bool PrivacyFilter::isEnabled() const
{
    return mEnabled;
}

void PrivacyFilter::setEnabled(bool enabled)
{
    mEnabled = enabled;
}

void PrivacyFilter::setUserName(const QString &userName)
{
    mUserName = userName;
}

void PrivacyFilter::setHomePath(const QString &homePath)
{
    mHomePath = homePath;
}

void PrivacyFilter::setHostName(const QString &hostName)
{
    mHostName = hostName;
}

void PrivacyFilter::addWordsProvider(const WordsProvider &provider)
{
    mWordsProviders.append(provider);
}

QStringList PrivacyFilter::sensitiveWords() const
{
    QStringList words;
    for (const WordsProvider &provider : mWordsProviders) {
        words += provider();
    }
    words.removeIf([](const QString &word) {
        return word.trimmed().size() < minimumWordLength;
    });
    words.removeDuplicates();
    // "Home Network 5G" before "Home Network"
    std::ranges::sort(words, [](const QString &left, const QString &right) {
        return left.size() > right.size();
    });
    return words;
}

QString PrivacyFilter::pseudonym(const QString &category, const QString &value)
{
    const QString key = category + u'\n' + value;
    if (const auto it = mPseudonyms.constFind(key); it != mPseudonyms.cend()) {
        return *it;
    }
    const QString result = u"<%1-%2>"_s.arg(category).arg(++mCounters[category]);
    mPseudonyms.insert(key, result);
    mOriginalValues.insert(result, value);
    return result;
}

QString PrivacyFilter::originalValue(const QString &pseudonym) const
{
    return mOriginalValues.value(pseudonym);
}

QJsonValue PrivacyFilter::restore(const QJsonValue &value) const
{
    switch (value.type()) {
    case QJsonValue::String: {
        static const QRegularExpression pseudonymExpression(u"<(?:mac|ip|name)-\\d+>|<user>|<hostname>"_s);
        QString text = value.toString();
        QRegularExpressionMatchIterator it = pseudonymExpression.globalMatch(text);
        // Replaced from the end: positions of previous matches don't change
        QList<QRegularExpressionMatch> matches;
        while (it.hasNext()) {
            matches.prepend(it.next());
        }
        for (const QRegularExpressionMatch &match : std::as_const(matches)) {
            QString original;
            if (match.captured() == "<user>"_L1) {
                original = mUserName;
            } else if (match.captured() == "<hostname>"_L1) {
                original = mHostName;
            } else {
                original = originalValue(match.captured());
            }
            // Unknown pseudonym is kept: action will report an unknown device
            if (!original.isEmpty()) {
                text.replace(match.capturedStart(), match.capturedLength(), original);
            }
        }
        return text;
    }
    case QJsonValue::Array: {
        QJsonArray array;
        const QJsonArray values = value.toArray();
        for (const QJsonValue &item : values) {
            array.append(restore(item));
        }
        return array;
    }
    case QJsonValue::Object: {
        QJsonObject object;
        const QJsonObject values = value.toObject();
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            object.insert(it.key(), restore(it.value()));
        }
        return object;
    }
    default:
        return value;
    }
}

QString PrivacyFilter::filter(const QString &text)
{
    if (!mEnabled || text.isEmpty()) {
        return text;
    }
    QString result = text;
    // Sensitive words first: a Wi-Fi network name can contain an address
    const QStringList words = sensitiveWords();
    for (const QString &word : words) {
        result = replaceMatches(result, wordExpression(word), [this](const QString &match) {
            return pseudonym(u"name"_s, match);
        });
    }

    if (mHomePath.size() > 1) {
        result.replace(mHomePath, u"~"_s);
    }

    // MAC addresses, also in names of Bluetooth devices ("bluez_output.AA_BB_CC_DD_EE_FF.1")
    static const QRegularExpression macExpression(u"(?<![0-9A-Fa-f])[0-9A-Fa-f]{2}([:_-])(?:[0-9A-Fa-f]{2}\\1){4}[0-9A-Fa-f]{2}(?![0-9A-Fa-f])"_s);
    result = replaceMatches(result, macExpression, [this](const QString &match) {
        QString normalized = match.toUpper();
        normalized.replace(u'_', u':').replace(u'-', u':');
        return pseudonym(u"mac"_s, normalized);
    });

    static const QRegularExpression ipv4Expression(u"(?<![\\d.])(?:\\d{1,3}\\.){3}\\d{1,3}(?![\\d.])"_s);
    result = replaceMatches(result, ipv4Expression, [this](const QString &match) {
        const QHostAddress address(match);
        // Loopback and unspecified addresses are useful to diagnose and don't identify anybody
        if (address.isNull() || address.isLoopback() || address == QHostAddress::AnyIPv4) {
            return QString();
        }
        return pseudonym(u"ip"_s, address.toString());
    });

    static const QRegularExpression ipv6Expression(u"(?<![0-9A-Fa-f:])(?:[0-9A-Fa-f]{0,4}:){2,7}[0-9A-Fa-f]{0,4}(?![0-9A-Fa-f:])"_s);
    result = replaceMatches(result, ipv6Expression, [this](const QString &match) {
        // Times ("12:30:45") are not valid addresses
        const QHostAddress address(match);
        if (address.isNull() || address.protocol() != QAbstractSocket::IPv6Protocol || address.isLoopback() || address == QHostAddress::AnyIPv6) {
            return QString();
        }
        return pseudonym(u"ip"_s, address.toString());
    });

    if (mHostName.size() >= minimumWordLength) {
        result.replace(wordExpression(mHostName), u"<hostname>"_s);
    }
    if (mUserName.size() >= minimumWordLength) {
        result.replace(wordExpression(mUserName), u"<user>"_s);
    }
    return result;
}

QJsonValue PrivacyFilter::filter(const QJsonValue &value)
{
    if (!mEnabled) {
        return value;
    }
    switch (value.type()) {
    case QJsonValue::String:
        return filter(value.toString());
    case QJsonValue::Array: {
        QJsonArray array;
        const QJsonArray values = value.toArray();
        for (const QJsonValue &item : values) {
            array.append(filter(item));
        }
        return array;
    }
    case QJsonValue::Object: {
        QJsonObject object;
        const QJsonObject values = value.toObject();
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            object.insert(filter(it.key()), filter(it.value()));
        }
        return object;
    }
    default:
        return value;
    }
}
