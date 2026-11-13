#include "app/Language.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QTranslator>

namespace Language {

namespace {
QString g_current = QStringLiteral("de");
}

QStringList available()
{
    return {QStringLiteral("de"), QStringLiteral("en")};
}

QString nativeName(const QString &language)
{
    if (language == QLatin1String("de"))
        return QStringLiteral("Deutsch");
    if (language == QLatin1String("en"))
        return QStringLiteral("English");
    return language;
}

QString resolve(const QString &setting, const QStringList &systemLanguages)
{
    if (available().contains(setting))
        return setting;
    for (const QString &tag : systemLanguages) {
        // "de-CH" -> "de"
        const QString language = tag.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
        if (available().contains(language))
            return language;
    }
    return QStringLiteral("en");
}

void install(QCoreApplication &app, const QString &language)
{
    g_current = available().contains(language) ? language : QStringLiteral("de");
    // Dates and numbers everywhere (QLocale()) in the UI's language too.
    QLocale::setDefault(locale());

    // The app's texts are German in the code; other languages come from
    // the .qm files that CMake builds into the resources (qt_add_translations).
    if (g_current != QLatin1String("de")) {
        auto *translator = new QTranslator(&app);
        if (translator->load(QStringLiteral(":/i18n/snapslash_%1.qm").arg(g_current)))
            app.installTranslator(translator);
    }

    // Qt's own texts (buttons of message boxes, context menus) are English
    // in Qt; other languages from Qt's translations, which windeployqt puts
    // next to the program on Windows and the macOS package script into the
    // bundle's Contents/Resources. Without them those few texts stay English.
    // windeployqt writes qt_<language>.qm, Qt's own folder has qtbase_ too.
    if (g_current != QLatin1String("en")) {
        auto *qt = new QTranslator(&app);
        const QStringList folders = {
#ifdef Q_OS_MACOS
            // Snapslash.app/Contents/MacOS is the program's folder.
            QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/translations"),
#endif
            QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath)};
        bool loaded = false;
        for (const QString &folder : folders) {
            for (const QString &name : {QStringLiteral("qt_%1"), QStringLiteral("qtbase_%1")}) {
                loaded = loaded || qt->load(name.arg(g_current), folder);
            }
        }
        if (loaded)
            app.installTranslator(qt);
        else
            delete qt;
    }
}

QString current()
{
    return g_current;
}

QLocale locale()
{
    return g_current == QLatin1String("de") ? QLocale(QLocale::German, QLocale::Switzerland)
                                              : QLocale(QLocale::English, QLocale::UnitedKingdom);
}

} // namespace Language
