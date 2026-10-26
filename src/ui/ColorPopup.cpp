#include "ui/ColorPopup.h"

#include "ui/ColorWheel.h"
#include "ui/Icons.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// "#293049", "293049" or the short form "#239"; invalid otherwise.
QColor parseHex(QString text)
{
    text = text.trimmed();
    if (!text.startsWith(QLatin1Char('#')))
        text.prepend(QLatin1Char('#'));
    if (text.size() != 4 && text.size() != 7)
        return {};
    return QColor::fromString(text);
}

QPixmap previewPixmap(const QColor &color)
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(0, 0, 0, 60));
    p.setBrush(color);
    p.drawRoundedRect(QRectF(0.5, 0.5, 27, 27), 6, 6);
    return pixmap;
}

} // namespace

ColorPopup::ColorPopup(QWidget *parent)
    : QFrame(parent, Qt::Popup)
    , m_wheel(new ColorWheel(this))
    , m_hex(new QLineEdit(this))
    , m_preview(new QLabel(this))
    , m_eyedropper(new QToolButton(this))
    , m_recent(new QWidget(this))
{
    setObjectName(QStringLiteral("ColorPopup"));
    setAttribute(Qt::WA_StyledBackground);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    m_wheel->setFixedSize(208, 208);
    layout->addWidget(m_wheel, 0, Qt::AlignHCenter);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    m_preview->setAccessibleName(tr("Gewählte Farbe"));
    row->addWidget(m_preview);
    auto *hash = new QLabel(QStringLiteral("#"), this);
    row->addWidget(hash);
    m_hex->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("#?[0-9A-Fa-f]{0,6}")), m_hex));
    m_hex->setAccessibleName(tr("Hex-Code"));
    m_hex->setToolTip(tr("Hex-Code, z. B. 293049"));
    row->addWidget(m_hex, 1);
    m_eyedropper->setToolTip(tr("Farbe aus dem Bild aufnehmen"));
    m_eyedropper->setAccessibleName(tr("Pipette"));
    m_eyedropper->setProperty("role", QStringLiteral("icon"));
    m_eyedropper->setIconSize(QSize(18, 18));
    row->addWidget(m_eyedropper);
    layout->addLayout(row);

    // Recently used own colours; filled by setRecentColors().
    auto *recentLayout = new QHBoxLayout(m_recent);
    recentLayout->setContentsMargins(0, 0, 0, 0);
    recentLayout->setSpacing(4);
    m_recent->hide();
    layout->addWidget(m_recent);

    connect(m_wheel, &ColorWheel::colorChanged, this, [this](const QColor &color) { userChose(color); });
    // A complete code applies while typing; the hex field keeps the text.
    connect(m_hex, &QLineEdit::textEdited, this, [this](const QString &text) {
        const QString digits = text.startsWith(QLatin1Char('#')) ? text.mid(1) : text;
        if (digits.size() == 6) {
            if (const QColor color = parseHex(text); color.isValid())
                userChose(color, false);
        }
    });
    connect(m_hex, &QLineEdit::editingFinished, this, [this] {
        if (const QColor color = parseHex(m_hex->text()); color.isValid() && color != m_color)
            userChose(color);
        else
            showColor(m_color); // back to the current code
    });
    connect(m_eyedropper, &QToolButton::clicked, this, [this] {
        hide();
        emit eyedropperRequested();
    });
}

void ColorPopup::setRecentColors(const QList<QColor> &colors)
{
    auto *recentLayout = static_cast<QHBoxLayout *>(m_recent->layout());
    while (QLayoutItem *item = recentLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const QColor &color : colors) {
        auto *button = new QToolButton(m_recent);
        button->setIcon(QIcon(previewPixmap(color)));
        button->setIconSize(QSize(20, 20));
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        const QString name = color.name().mid(1).toUpper();
        button->setToolTip(name);
        button->setAccessibleName(tr("Zuletzt verwendet: %1").arg(name));
        connect(button, &QToolButton::clicked, this, [this, color] { userChose(color); });
        recentLayout->addWidget(button);
    }
    recentLayout->addStretch(1);
    m_recent->setVisible(!colors.isEmpty());
}

void ColorPopup::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    emit closed();
}

void ColorPopup::setIconColor(const QColor &color)
{
    m_eyedropper->setIcon(Icons::icon(QStringLiteral("eyedropper"), color));
}

void ColorPopup::showColor(const QColor &color, bool updateHex)
{
    m_color = color;
    m_wheel->setColor(color);
    if (updateHex)
        m_hex->setText(color.name(QColor::HexRgb).mid(1).toUpper());
    m_preview->setPixmap(previewPixmap(color));
}

void ColorPopup::userChose(const QColor &color, bool updateHex)
{
    showColor(color, updateHex);
    emit colorChanged(color);
}

void ColorPopup::showAt(QWidget *anchor, const QColor &color)
{
    showColor(color.isValid() ? color : QColor(Qt::black));
    adjustSize();
    // Below the anchor, aligned to its right edge, kept on the screen.
    QPoint pos = anchor->mapToGlobal(QPoint(anchor->width() - width(), anchor->height() + 4));
    if (const QScreen *screen = anchor->screen()) {
        const QRect area = screen->availableGeometry();
        if (pos.y() + height() > area.bottom())
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 4);
        pos.setX(std::clamp(pos.x(), area.left(), std::max(area.left(), area.right() - width())));
    }
    move(pos);
    show();
    m_hex->setFocus();
    m_hex->selectAll();
}
