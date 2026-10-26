#include "app/FileNames.h"

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
    return QStringLiteral("{Datum}_{Zeit}_{Titel}");
}

QString fromPattern(const QString &pattern, const QString &title, const QDateTime &time)
{
    QString name = pattern.trimmed().isEmpty() ? defaultPattern() : pattern;
    name.replace(QStringLiteral("{Datum}"), time.toString(QStringLiteral("yyyy-MM-dd")), Qt::CaseInsensitive);
    name.replace(QStringLiteral("{Zeit}"), time.toString(QStringLiteral("HH-mm-ss")), Qt::CaseInsensitive);
    name.replace(QStringLiteral("{Titel}"), slug(title).left(60), Qt::CaseInsensitive);
    name = slug(name);
    if (name.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
        name.chop(4);
    if (name.isEmpty())
        name = QStringLiteral("screenshot");
    return name.left(120) + QStringLiteral(".png");
}

QString defaultFolder()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return QDir(pictures.isEmpty() ? QDir::homePath() : pictures).filePath(QStringLiteral("NSL Snap"));
}

} // namespace FileNames
