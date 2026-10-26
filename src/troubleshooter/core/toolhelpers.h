/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerTool>
#include <functional>

namespace TextAutoGenerateTextMcpProtocolCore
{
class McpProtocolServerProtocolManager;
class McpProtocolServerToolCall;
}

namespace ToolHelpers
{
// Defines annotations sent with tool (client uses them to ask a confirmation)
enum class ToolKind : uint8_t {
    Diagnostic, // read only
    ReversibleAction, // modifies system, can be undone
    DestructiveAction, // modifies system, can't be undone
};

// Tools are grouped by client (_meta org.kde/group and org.kde/groupTitle)
struct ToolGroup {
    QString name;
    QString title;
};

// Tool which calls a function: avoids to create a class by tool
class FunctionTool : public TextAutoGenerateTextMcpProtocolCore::McpProtocolServerTool
{
public:
    using Handler = std::function<void(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerToolCall *)>;
    FunctionTool(const TextAutoGenerateTextMcpProtocolCore::McpProtocolTool &definition, Handler handler);
    ~FunctionTool() override;

    [[nodiscard]] TextAutoGenerateTextMcpProtocolCore::McpProtocolTool definition() const override;
    void call(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerToolCall *call) override;

private:
    const TextAutoGenerateTextMcpProtocolCore::McpProtocolTool mDefinition;
    const Handler mHandler;
};

// Description is in English (better understood by language models), title is translated
[[nodiscard]] TextAutoGenerateTextMcpProtocolCore::McpProtocolTool createTool(const QString &name,
                                                                              const QString &title,
                                                                              const QString &description,
                                                                              ToolKind kind,
                                                                              const ToolGroup &group,
                                                                              const QMap<QString, QJsonObject> &properties = {},
                                                                              const QStringList &required = {});

void addFunctionTool(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager,
                     const TextAutoGenerateTextMcpProtocolCore::McpProtocolTool &definition,
                     FunctionTool::Handler handler);

// Answers with \a result as structuredContent, and with \a summary and serialized \a result as text contents:
// clients which ignore structuredContent (e.g. ktextaddons) give only text to language model
void finishWithJson(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerToolCall *call, const QJsonObject &result, const QString &summary);
}
