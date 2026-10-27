/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
}

// Guided scenarios (MCP prompts): they tell language model which tools to call and in which order.
// _meta contains icon (org.kde/icon) and diagnostic tools without argument (org.kde/diagnosticTools) used without language model.
namespace ScenarioPrompts
{
struct Scenario {
    QString name;
    QString title; // translated
    QString description; // English
    QString iconName;
    // Request sent as user message (English)
    QString request;
    // Tools to call in order, with their purpose
    QStringList steps;
    // Typical fixes (tools or advice)
    QStringList fixes;
    // Tools called by quick diagnostic (without language model): no required argument
    QStringList diagnosticTools;
};

[[nodiscard]] QList<Scenario> scenarios();
// Text of prompt: request, details of user, steps, fixes and rules
[[nodiscard]] QString promptText(const Scenario &scenario, const QString &details, const QString &language);

void registerPrompts(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager);
}
