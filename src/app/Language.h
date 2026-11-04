#pragma once

#include <QLocale>
#include <QString>
#include <QStringList>

class QCoreApplication;

// The language of the user interface (D-038): German, the language the
// texts are written in, or English from translations/snapslash_en.ts. The
// setting "Sprache" chooses; by default the system's language decides.
namespace Language {

// The languages the app has, as setting values; "" follows the system.
QStringList available(); // {"de", "en"}
// "Deutsch", "English": each in its own language.
QString nativeName(const QString &language);

// The language to use for a setting ("", "de" or "en") and the system's
// preferred languages (QLocale::uiLanguages(), e.g. {"de-CH", "en-US"}):
// the first available one in the system's list, else English.
QString resolve(const QString &setting, const QStringList &systemLanguages);

// Loads the app's and Qt's own translations for `language` into `app`.
// Call once at start, before any window exists: texts are taken when a
// widget is built, so a change applies after a restart.
void install(QCoreApplication &app, const QString &language);

// The language installed at start ("de" before install()), and its locale
// for dates and sizes (de_CH or en_GB).
QString current();
QLocale locale();

} // namespace Language
