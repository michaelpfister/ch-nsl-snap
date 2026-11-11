#include "app/FileNames.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

namespace FileNames {

namespace {

// Keeps letters, digits, ".", "-" and "_"; runs of anything else become one "_".
QString slug(const QString &text)
{
    QString result = text;
    result.replace(QRegularExpression(QStringLiteral(R"([^\w.-]+)")), QStringLiteral("_"));
    while (result.startsWith(u'_') || result.startsWith(u'.'))
        result.remove(0, 1);
    while (result.endsWith(u'_') || result.endsWith(u'.'))
        result.chop(1);
    return result;
}

} // namespace

QString defaultPattern()
{
    // The placeholders in the UI's language; both sets work (fromPattern).
    return QCoreApplication::translate("FileNames", "{Datum}_{Zeit}_{Titel}");
}

QString fromPattern(const QString &pattern, const QString &title, const QDateTime &time, const QString &extension)
{
    QString name = pattern.trimmed().isEmpty() ? defaultPattern() : pattern;
    name.replace(QStringLiteral("{Datum}"), time.toString(QStringLiteral("yyyy-MM-dd")), Qt::CaseInsensitive);
    name.replace(QStringLiteral("{Zeit}"), time.toString(QStringLiteral("HH-mm-ss")), Qt::CaseInsensitive);
    name.replace(QStringLiteral("{Titel}"), slug(title).left(60), Qt::CaseInsensitive);
    // The English placeholders (D-038), so a pattern works in either language.
    name.replace(QStringLiteral("{Date}"), time.toString(QStringLiteral("yyyy-MM-dd")), Qt::CaseInsensitive);
    name.replace(QStringLiteral("{Time}"), time.toString(QStringLiteral("HH-mm-ss")), Qt::CaseInsensitive);
    name.replace(QStringLiteral("{Title}"), slug(title).left(60), Qt::CaseInsensitive);
    name = slug(name);
    const QString suffix = QLatin1Char('.') + extension;
    if (name.endsWith(suffix, Qt::CaseInsensitive))
        name.chop(suffix.size());
    if (name.isEmpty())
        name = QStringLiteral("screenshot");
    return name.left(120) + suffix;
}

QString defaultFolder()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return QDir(pictures.isEmpty() ? QDir::homePath() : pictures).filePath(QStringLiteral("Snapslash"));
}

} // namespace FileNames
