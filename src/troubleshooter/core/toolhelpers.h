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
#include <optional>

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

// Description is in English (better understood by language models), title is translated.
// \a openWorld: tool contacts external hosts
[[nodiscard]] TextAutoGenerateTextMcpProtocolCore::McpProtocolTool createTool(const QString &name,
                                                                              const QString &title,
                                                                              const QString &description,
                                                                              ToolKind kind,
                                                                              const ToolGroup &group,
                                                                              const QMap<QString, QJsonObject> &properties = {},
                                                                              const QStringList &required = {},
                                                                              bool openWorld = false);

void addFunctionTool(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager,
                     const TextAutoGenerateTextMcpProtocolCore::McpProtocolTool &definition,
                     FunctionTool::Handler handler);

// Calls tool \a name registered with addFunctionTool() without client (e.g. to build a report).
// \a callback receives MCP result (content, structuredContent, isError), it's not called if \a context is deleted.
void callTool(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerProtocolManager *manager,
              const QString &name,
              const QMap<QString, QJsonValue> &arguments,
              QObject *context,
              const std::function<void(const QJsonObject &result)> &callback);

// Language models often send numbers and booleans as strings ("60", "true"): they are accepted.
// Daemon uses same rules to show arguments to user.
[[nodiscard]] std::optional<qint64> integerArgument(const QJsonValue &value);
[[nodiscard]] std::optional<double> numberArgument(const QJsonValue &value);
[[nodiscard]] std::optional<bool> booleanArgument(const QJsonValue &value);

// Answers with error \a message (personal data are masked)
void finishWithError(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerToolCall *call, const QString &message);

// Personal data are masked by PrivacyFilter.
// Answers with \a result as structuredContent, and with \a summary and serialized \a result as text contents:
// clients which ignore structuredContent (e.g. ktextaddons) give only text to language model
void finishWithJson(TextAutoGenerateTextMcpProtocolCore::McpProtocolServerToolCall *call, const QJsonObject &result, const QString &summary);
}
