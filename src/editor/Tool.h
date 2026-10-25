#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

// The editor's tools (docs/04-features.md). Only the metadata lives here;
// the behaviour of each tool comes with its implementation.
enum class ToolId {
    Select,
    Crop,
    Frame,
    Number,
    Text,
    Arrow,
    Marker,
    Pixelate,
    ReplaceText,
};

struct ToolInfo
{
    ToolId id;
    QString icon;          // resources/icons/<icon>.svg
    QString name;          // tooltip and panel title
    QKeySequence shortcut; // single key, F-46
    QString hint;          // status bar hint while the tool is active
    bool separatorBefore = false;
};

// All tools in tool-rail order.
const QList<ToolInfo> &allTools();
const ToolInfo &toolInfo(ToolId id);
