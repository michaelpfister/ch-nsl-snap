#include "ui/Motion.h"

#include <QProcess>
#include <QStandardPaths>
#include <QString>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Motion {

namespace {

bool askSystem()
{
#ifdef Q_OS_WIN
    // "Animationseffekte" in Settings › Accessibility › Visual effects.
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return !animations;
    return false;
#else
    // GNOME's switch, also read on other desktops that have the schema
    // (Zorin, Ubuntu). No gsettings (Hyprland without GNOME): animate.
    if (QStandardPaths::findExecutable(QStringLiteral("gsettings")).isEmpty())
        return false;
    QProcess process;
    process.start(QStringLiteral("gsettings"),
                  {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("enable-animations")});
    if (!process.waitForFinished(1000) || process.exitCode() != 0)
        return false;
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed() == QLatin1String("false");
#endif
}

} // namespace

bool reduced()
{
    const QByteArray override = qgetenv("NSL_REDUCED_MOTION");
    if (override == "1")
        return true;
    if (override == "0")
        return false;
    static const bool answer = askSystem();
    return answer;
}

} // namespace Motion
