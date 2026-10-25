#include "ui/Icons.h"

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace Icons {

namespace {

QPixmap render(const QString &name, const QColor &color, int size)
{
    QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name().toLatin1());

    // Render at device resolution so icons stay sharp on HiDPI screens.
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

} // namespace

QIcon icon(const QString &name, const QColor &color)
{
    QIcon result;
    for (int size : {16, 20, 24, 32})
        result.addPixmap(render(name, color, size), QIcon::Normal);
    return result;
}

QIcon toggleIcon(const QString &name, const QColor &normal, const QColor &checked)
{
    QIcon result;
    for (int size : {16, 20, 24, 32}) {
        result.addPixmap(render(name, normal, size), QIcon::Normal, QIcon::Off);
        result.addPixmap(render(name, checked, size), QIcon::Normal, QIcon::On);
    }
    return result;
}

} // namespace Icons
