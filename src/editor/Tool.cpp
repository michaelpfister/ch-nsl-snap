#include "editor/Tool.h"

#include <QCoreApplication>

// Not in a QObject: translated in the context "Tool". The full call (not a
// helper) lets lupdate find the texts for translations/snapslash_en.ts.

const QList<ToolInfo> &allTools()
{
    static const QList<ToolInfo> tools = {
        {ToolId::Select, QStringLiteral("select"), QCoreApplication::translate("Tool", "Auswählen"), QKeySequence(Qt::Key_V),
         QCoreApplication::translate("Tool", "Objekt anklicken zum Bearbeiten · Entf zum Löschen")},
        {ToolId::Crop, QStringLiteral("crop"), QCoreApplication::translate("Tool", "Zuschneiden"), QKeySequence(Qt::Key_C),
         QCoreApplication::translate("Tool", "Ziehen zum Zuschneiden · Enter bestätigt")},
        {ToolId::Frame, QStringLiteral("frame"), QCoreApplication::translate("Tool", "Rahmen"), QKeySequence(Qt::Key_R),
         QCoreApplication::translate("Tool", "Ziehen zum Zeichnen · Shift für Quadrat"), true},
        {ToolId::Number, QStringLiteral("number"), QCoreApplication::translate("Tool", "Nummerierter Rahmen"), QKeySequence(Qt::Key_N),
         QCoreApplication::translate("Tool", "Ziehen zum Zeichnen · Nummer wird automatisch vergeben")},
        {ToolId::Text, QStringLiteral("text"), QCoreApplication::translate("Tool", "Text"), QKeySequence(Qt::Key_T),
         QCoreApplication::translate("Tool", "Klicken zum Schreiben · Doppelklick auf einen Text bearbeitet ihn")},
        {ToolId::Arrow, QStringLiteral("arrow"), QCoreApplication::translate("Tool", "Pfeil"), QKeySequence(Qt::Key_A),
         QCoreApplication::translate("Tool", "Ziehen von Anfang bis Spitze · Shift für 45°")},
        {ToolId::Marker, QStringLiteral("marker"), QCoreApplication::translate("Tool", "Marker"), QKeySequence(Qt::Key_M),
         QCoreApplication::translate("Tool", "Über Text ziehen")},
        {ToolId::Pixelate, QStringLiteral("pixelate"), QCoreApplication::translate("Tool", "Verpixeln / Weichzeichnen"), QKeySequence(Qt::Key_P),
         QCoreApplication::translate("Tool", "Ziehen zum Verpixeln · Alt für Weichzeichnen")},
        {ToolId::ReplaceText, QStringLiteral("replace"), QCoreApplication::translate("Tool", "Text ersetzen"), QKeySequence(Qt::Key_E),
         QCoreApplication::translate("Tool", "Text markieren · neuen Text tippen")},
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
