/*
  SPDX-FileCopyrightText: 2026 Laurent Montel <montel@kde.org>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "toolhelpers.h"
#include "privacyfilter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolCallToolResult>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolCommonUtils>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolMeta>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerProtocolManager>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolServerToolCall>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolTextContent>
#include <TextAutoGenerateTextMcpProtocolCore/McpProtocolToolAnnotations>

using namespace Qt::Literals::StringLiterals;
using namespace TextAutoGenerateTextMcpProtocolCore;

namespace
{
// Handlers of tools, kept to call them without client (child of manager)
class ToolRegistry : public QObject
{
public:
    explicit ToolRegistry(QObject *parent)
        : QObject(parent)
    {
        setObjectName(name());
    }
    [[nodiscard]] static QString name()
    {
        return u"plasma-assist-tool-registry"_s;
    }
    [[nodiscard]] static ToolRegistry *registry(McpProtocolServerProtocolManager *manager)
    {
        if (auto registry = dynamic_cast<ToolRegistry *>(manager->findChild<QObject *>(name(), Qt::FindDirectChildrenOnly)); registry) {
            return registry;
        }
        return new ToolRegistry(manager);
    }
    QHash<QString, ToolHelpers::FunctionTool::Handler> handlers;
};

QJsonObject errorResult(const QString &message)
{
    return QJsonObject{
        {u"isError"_s, true},
        {u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, message}}}},
    };
}
}

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
                                        const QStringList &required,
                                        bool openWorld)
{
    McpProtocolTool tool = McpProtocolServerTool::createTool(name, description, properties, required);
    tool.setTitle(title);

    // Missing annotations are considered as destructive by client: always set them
    McpProtocolToolAnnotations annotations;
    annotations.setTitle(title);
    annotations.setOpenWorldHint(openWorld);
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
    ToolRegistry::registry(manager)->handlers.insert(definition.name(), handler);
    manager->addTool(std::make_unique<FunctionTool>(definition, std::move(handler)));
}

void ToolHelpers::callTool(McpProtocolServerProtocolManager *manager,
                           const QString &name,
                           const QMap<QString, QJsonValue> &arguments,
                           QObject *context,
                           const std::function<void(const QJsonObject &result)> &callback)
{
    const FunctionTool::Handler handler = ToolRegistry::registry(manager)->handlers.value(name);
    if (!handler) {
        callback(errorResult(u"Unknown tool: %1"_s.arg(name)));
        return;
    }
    auto call = new McpProtocolServerToolCall(QJsonValue(0), name, arguments, context);
    QObject::connect(call, &McpProtocolServerToolCall::resultReady, context, [call, callback](const QJsonObject &result) {
        call->deleteLater();
        callback(result);
    });
    QObject::connect(call, &McpProtocolServerToolCall::errorReady, context, [call, callback](int, const QString &message) {
        call->deleteLater();
        callback(errorResult(message));
    });
    handler(call);
}

std::optional<qint64> ToolHelpers::integerArgument(const QJsonValue &value)
{
    if (value.isDouble()) {
        return value.toInteger();
    }
    if (value.isString()) {
        bool ok = false;
        const qint64 result = value.toString().trimmed().toLongLong(&ok);
        if (ok) {
            return result;
        }
    }
    return std::nullopt;
}

std::optional<double> ToolHelpers::numberArgument(const QJsonValue &value)
{
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        bool ok = false;
        const double result = value.toString().trimmed().toDouble(&ok);
        if (ok) {
            return result;
        }
    }
    return std::nullopt;
}

std::optional<bool> ToolHelpers::booleanArgument(const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool();
    }
    if (const QString text = value.toString().trimmed().toLower(); text == "true"_L1 || text == "false"_L1) {
        return text == "true"_L1;
    }
    return std::nullopt;
}

void ToolHelpers::finishWithError(McpProtocolServerToolCall *call, const QString &message)
{
    call->finishWithText(PrivacyFilter::instance()->filter(message), true);
}

void ToolHelpers::finishWithJson(McpProtocolServerToolCall *call, const QJsonObject &unfilteredResult, const QString &summary)
{
    PrivacyFilter *filter = PrivacyFilter::instance();
    const QJsonObject result = filter->filter(unfilteredResult).toObject();
    McpProtocolTextContent content;
    content.setText(filter->filter(summary));
    McpProtocolTextContent jsonContent;
    jsonContent.setText(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    QMap<QString, QJsonValue> structuredContent;
    for (auto it = result.constBegin(); it != result.constEnd(); ++it) {
        structuredContent.insert(it.key(), it.value());
    }
    McpProtocolCallToolResult toolResult;
    toolResult.setContent({content, jsonContent});
    toolResult.setStructuredContent(structuredContent);
    call->finish(toolResult);
}
