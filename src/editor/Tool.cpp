#include "editor/Tool.h"

#include <QCoreApplication>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Tool", text);
}

} // namespace

const QList<ToolInfo> &allTools()
{
    static const QList<ToolInfo> tools = {
        {ToolId::Select, QStringLiteral("select"), tr("Auswählen"), QKeySequence(Qt::Key_V),
         tr("Objekt anklicken zum Bearbeiten · Entf zum Löschen")},
        {ToolId::Crop, QStringLiteral("crop"), tr("Zuschneiden"), QKeySequence(Qt::Key_C),
         tr("Ziehen zum Zuschneiden · Enter bestätigt")},
        {ToolId::Frame, QStringLiteral("frame"), tr("Rahmen"), QKeySequence(Qt::Key_R),
         tr("Ziehen zum Zeichnen · Shift für Quadrat"), true},
        {ToolId::Number, QStringLiteral("number"), tr("Nummerierter Rahmen"), QKeySequence(Qt::Key_N),
         tr("Ziehen zum Zeichnen · Nummer wird automatisch vergeben")},
        {ToolId::Text, QStringLiteral("text"), tr("Text"), QKeySequence(Qt::Key_T),
         tr("Klicken zum Schreiben")},
        {ToolId::Arrow, QStringLiteral("arrow"), tr("Pfeil"), QKeySequence(Qt::Key_A),
         tr("Ziehen von Anfang bis Spitze")},
        {ToolId::Marker, QStringLiteral("marker"), tr("Marker"), QKeySequence(Qt::Key_M),
         tr("Über Text ziehen")},
        {ToolId::Pixelate, QStringLiteral("pixelate"), tr("Verpixeln / Weichzeichnen"), QKeySequence(Qt::Key_P),
         tr("Ziehen zum Verpixeln · Alt für Weichzeichnen")},
        {ToolId::ReplaceText, QStringLiteral("replace"), tr("Text ersetzen"), QKeySequence(Qt::Key_E),
         tr("Text markieren · neuen Text tippen")},
    };
    return tools;
}

const ToolInfo &toolInfo(ToolId id)
{
    for (const ToolInfo &tool : allTools()) {
        if (tool.id == id)
            return tool;
    }
    return allTools().first();
}
