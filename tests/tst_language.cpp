// The user interface in German and English (D-038): which language is
// chosen, that the English translation loads, and that it is complete.

#include "app/FileNames.h"
#include "app/Language.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTest>
#include <QXmlStreamReader>

class TestLanguage : public QObject
{
    Q_OBJECT

private slots:
    void settingOrSystemDecides()
    {
        const QStringList swiss = {QStringLiteral("de-CH"), QStringLiteral("en-US")};
        QCOMPARE(Language::resolve(QStringLiteral("en"), swiss), QStringLiteral("en"));
        QCOMPARE(Language::resolve(QStringLiteral("de"), {QStringLiteral("en-US")}), QStringLiteral("de"));
        // Following the system: its first language the app has.
        QCOMPARE(Language::resolve(QString(), swiss), QStringLiteral("de"));
        QCOMPARE(Language::resolve(QString(), {QStringLiteral("fr-CH"), QStringLiteral("de_DE")}), QStringLiteral("de"));
        QCOMPARE(Language::resolve(QString(), {QStringLiteral("en-GB")}), QStringLiteral("en"));
        // Neither German nor English: English.
        QCOMPARE(Language::resolve(QString(), {QStringLiteral("fr-FR")}), QStringLiteral("en"));
        QCOMPARE(Language::resolve(QStringLiteral("xx"), {}), QStringLiteral("en"));
    }

    void fileNamesUnderstandBothLanguages()
    {
        const QDateTime time(QDate(2026, 11, 4), QTime(9, 5, 7));
        QCOMPARE(FileNames::fromPattern(QStringLiteral("{Date}_{Time}_{Title}"), QStringLiteral("Settings"), time),
                 QStringLiteral("2026-11-04_09-05-07_Settings.png"));
        QCOMPARE(FileNames::fromPattern(QStringLiteral("{Datum}_{Title}"), QStringLiteral("x"), time),
                 QStringLiteral("2026-11-04_x.png"));
    }

    // Last: installs the English translation into this test's application.
    void englishTranslationLoads()
    {
        QCOMPARE(FileNames::defaultPattern(), QStringLiteral("{Datum}_{Zeit}_{Titel}"));
        Language::install(*QCoreApplication::instance(), QStringLiteral("en"));
        QCOMPARE(Language::current(), QStringLiteral("en"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "Kopieren"), QStringLiteral("Copy"));
        QCOMPARE(FileNames::defaultPattern(), QStringLiteral("{Date}_{Time}_{Title}"));
        QCOMPARE(Language::locale().language(), QLocale::English);
    }

    // Every text has a finished translation, with the same placeholders.
    void translationIsComplete()
    {
        QFile file(QStringLiteral(NSL_SOURCE_DIR "/translations/snapslash_en.ts"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QXmlStreamReader xml(&file);
        QString source;
        int messages = 0;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;
            if (xml.name() == QLatin1String("source")) {
                source = xml.readElementText();
            } else if (xml.name() == QLatin1String("translation")) {
                const QString type = xml.attributes().value(QLatin1String("type")).toString();
                const QString translation = xml.readElementText();
                ++messages;
                QVERIFY2(type.isEmpty(), qPrintable(QStringLiteral("not translated: ") + source));
                QVERIFY2(!translation.isEmpty(), qPrintable(QStringLiteral("empty: ") + source));
                for (int n = 1; n <= 9; ++n) {
                    const QString placeholder = QStringLiteral("%%1").arg(n);
                    QVERIFY2(source.contains(placeholder) == translation.contains(placeholder),
                             qPrintable(QStringLiteral("placeholder %1 differs: ").arg(placeholder) + source));
                }
            }
        }
        QVERIFY(!xml.hasError());
        QVERIFY(messages > 300);
    }
};

QTEST_MAIN(TestLanguage)
#include "tst_language.moc"
