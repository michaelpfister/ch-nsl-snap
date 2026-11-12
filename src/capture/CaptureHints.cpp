#include "capture/CaptureHints.h"

#include <QCoreApplication>

// Not in a QObject: translated in the context "CaptureHints".
namespace CaptureHints {

QString window()
{
    return QCoreApplication::translate("CaptureHints", "Fenster anklicken · R: Bereich · S: Bildschirm · Esc: Abbrechen");
}

QString screens()
{
    return QCoreApplication::translate(
        "CaptureHints", "Bildschirm anklicken · Enter: alle Bildschirme · W: Fenster · R: Bereich · Esc: Abbrechen");
}

QString screen()
{
    return QCoreApplication::translate(
        "CaptureHints", "Klicken oder Enter: Bildschirm aufnehmen · W: Fenster · R: Bereich · Esc: Abbrechen");
}

QString regionFirstCorner()
{
    return QCoreApplication::translate("CaptureHints", "Erste Ecke anklicken · Pfeiltasten: 1 Pixel · W: Fenster · Esc: Abbrechen");
}

QString regionSecondCorner()
{
    return QCoreApplication::translate("CaptureHints", "Gegenüberliegende Ecke anklicken · Rücktaste: neu beginnen · Esc: Abbrechen");
}

QString regionDrag()
{
    return QCoreApplication::translate(
        "CaptureHints", "Bereich aufziehen · Pfeiltasten und Enter: pixelgenau · W: Fenster · Esc: Abbrechen");
}

QString countdown(bool clickCancels)
{
    if (!clickCancels)
        return QCoreApplication::translate("CaptureHints", "Esc: Abbrechen");
    return QCoreApplication::translate("CaptureHints", "Esc oder Klick: Abbrechen");
}

QString countdownTitle()
{
    return QCoreApplication::translate("CaptureHints", "Gleich wird aufgenommen");
}

QString videoWindow()
{
    return QCoreApplication::translate("CaptureHints", "Video: Fenster anklicken · S: Bildschirm · Esc: Abbrechen");
}

QString videoScreens()
{
    return QCoreApplication::translate(
        "CaptureHints", "Video: Bildschirm anklicken · Enter: alle Bildschirme · W: Fenster · Esc: Abbrechen");
}

QString videoScreen()
{
    return QCoreApplication::translate("CaptureHints",
                                       "Video: klicken oder Enter, um den Bildschirm aufzunehmen · W: Fenster · Esc: Abbrechen");
}

QVariantMap forExtension(bool countdownClickCancels)
{
    return {
        {QStringLiteral("window"), window()},
        {QStringLiteral("screens"), screens()},
        {QStringLiteral("screen"), screen()},
        {QStringLiteral("region-first"), regionFirstCorner()},
        {QStringLiteral("region-second"), regionSecondCorner()},
        {QStringLiteral("region-drag"), regionDrag()},
        {QStringLiteral("countdown"), countdown(countdownClickCancels)},
        {QStringLiteral("countdown-title"), countdownTitle()},
    };
}

} // namespace CaptureHints
