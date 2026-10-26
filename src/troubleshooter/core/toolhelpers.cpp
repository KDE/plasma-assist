/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "toolhelpers.h"
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolCallToolResult>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolCommonUtils>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolMeta>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolTextContent>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolToolAnnotations>

using namespace TextAutoGenerateTextMcpProtocolCore;

ToolHelpers::FunctionTool::FunctionTool(const McpProtocolTool &definition, Handler handler)
    : mDefinition(definition)
    , mHandler(std::move(handler))
{
}

ToolHelpers::FunctionTool::~FunctionTool() = default;

McpProtocolTool ToolHelpers::FunctionTool::definition() const
{
    return mDefinition;
}

void ToolHelpers::FunctionTool::call(McpProtocolServerToolCall *call)
{
    mHandler(call);
}

McpProtocolTool ToolHelpers::createTool(const QString &name,
                                        const QString &title,
                                        const QString &description,
                                        ToolKind kind,
                                        const ToolGroup &group,
                                        const QMap<QString, QJsonObject> &properties,
                                        const QStringList &required)
{
    McpProtocolTool tool = McpProtocolServerTool::createTool(name, description, properties, required);
    tool.setTitle(title);

    // Missing annotations are considered as destructive by client: always set them
    McpProtocolToolAnnotations annotations;
    annotations.setTitle(title);
    annotations.setOpenWorldHint(false);
    switch (kind) {
    case ToolKind::Diagnostic:
        annotations.setReadOnlyHint(true);
        break;
    case ToolKind::ReversibleAction:
        annotations.setReadOnlyHint(false);
        annotations.setDestructiveHint(false);
        annotations.setIdempotentHint(true);
        break;
    case ToolKind::DestructiveAction:
        annotations.setReadOnlyHint(false);
        annotations.setDestructiveHint(true);
        break;
    }
    tool.setAnnotations(annotations);

    McpProtocolMeta meta;
    meta.setMeta(QMap<QString, QJsonValue>{{McpProtocolCommonUtils::metaGroupName(), group.name}, {McpProtocolCommonUtils::metaGroupTitle(), group.title}});
    tool.setMeta(meta);
    return tool;
}

void ToolHelpers::addFunctionTool(McpProtocolServerProtocolManager *manager, const McpProtocolTool &definition, FunctionTool::Handler handler)
{
    manager->addTool(std::make_unique<FunctionTool>(definition, std::move(handler)));
}

void ToolHelpers::finishWithJson(McpProtocolServerToolCall *call, const QJsonObject &result, const QString &summary)
{
    McpProtocolTextContent content;
    content.setText(summary);
    QMap<QString, QJsonValue> structuredContent;
    for (auto it = result.constBegin(); it != result.constEnd(); ++it) {
        structuredContent.insert(it.key(), it.value());
    }
    McpProtocolCallToolResult toolResult;
    toolResult.setContent({content});
    toolResult.setStructuredContent(structuredContent);
    call->finish(toolResult);
}
