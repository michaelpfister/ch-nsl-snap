#include "app/RecentCaptures.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Item data: the file, and whether its thumbnail was read already.
constexpr int kPathRole = Qt::UserRole;
constexpr int kLoadedRole = Qt::UserRole + 1;

QToolButton *headerButton(const QString &tooltip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setProperty("role", QStringLiteral("icon"));
    button->setIconSize(QSize(16, 16));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

RecentCaptures::RecentCaptures(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RecentCaptures"));
    // A plain QWidget draws the style sheet's background only with this.
    setAttribute(Qt::WA_StyledBackground);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(8, 2, 8, 2);
    column->setSpacing(0);

    // Header: open/close, pin, clear.
    auto *header = new QHBoxLayout;
    header->setSpacing(2);
    m_toggle = new QToolButton(this);
    m_toggle->setObjectName(QStringLiteral("RecentToggle"));
    m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toggle->setIconSize(QSize(14, 14));
    m_toggle->setFocusPolicy(Qt::NoFocus);
    m_pin = headerButton(tr("Anheften: Die Liste bleibt offen"), this);
    m_pin->setCheckable(true);
    m_clear = headerButton(tr("Liste leeren (die Dateien bleiben)"), this);
    header->addWidget(m_toggle);
    header->addStretch(1);
    header->addWidget(m_pin);
    header->addWidget(m_clear);
    column->addLayout(header);

    // The thumbnails in one row, scrolling sideways.
    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(false);
    m_list->setMovement(QListView::Static);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setIconSize(QSize(kThumbWidth, kThumbHeight));
    m_list->setGridSize(QSize(kThumbWidth + 16, kThumbHeight + 30));
    m_list->setUniformItemSizes(true);
    m_list->setTextElideMode(Qt::ElideMiddle);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setFixedHeight(kThumbHeight + 30 + 14); // one row and the scroll bar
    m_list->setAccessibleName(tr("Letzte Aufnahmen"));
    column->addWidget(m_list);

    m_loader = new QTimer(this);
    m_loader->setInterval(0);
    connect(m_loader, &QTimer::timeout, this, &RecentCaptures::loadNextThumbnail);

    connect(m_toggle, &QToolButton::clicked, this, [this] {
        // Closing a pinned list unpins it.
        if (m_pinned) {
            setPinned(false);
            emit pinnedChanged(false);
        }
        setExpanded(!m_expanded);
    });
    connect(m_pin, &QToolButton::toggled, this, [this](bool pinned) {
        if (pinned == m_pinned)
            return;
        setPinned(pinned);
        emit pinnedChanged(pinned);
    });
    connect(m_clear, &QToolButton::clicked, this, [this] {
        m_list->clear();
        updateHeader();
        emit pathsChanged({});
    });
    // One click opens, as asked for; Enter on the keyboard too.
    connect(m_list, &QListWidget::itemClicked, this, &RecentCaptures::open);
    connect(m_list, &QListWidget::itemActivated, this, &RecentCaptures::open);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &RecentCaptures::showMenu);

    setExpanded(false);
    updateHeader();
}

void RecentCaptures::setPaths(const QStringList &paths)
{
    m_list->clear();
    for (const QString &path : paths) {
        if (QFileInfo::exists(path))
            m_list->addItem(createItem(path));
    }
    updateHeader();
    if (m_expanded)
        m_loader->start();
}

QStringList RecentCaptures::paths() const
{
    QStringList result;
    for (int i = 0; i < m_list->count(); ++i)
        result << m_list->item(i)->data(kPathRole).toString();
    return result;
}

void RecentCaptures::addCapture(const QString &path, const QImage &image)
{
    removePath(path);
    QListWidgetItem *item = createItem(path);
    m_list->insertItem(0, item);
    if (!image.isNull())
        setThumbnail(item, image);
    m_list->scrollToItem(item);
    updateHeader();
    if (m_expanded)
        m_loader->start();
    emit pathsChanged(paths());
}

void RecentCaptures::removePath(const QString &path)
{
    for (int i = m_list->count() - 1; i >= 0; --i) {
        if (m_list->item(i)->data(kPathRole).toString() == path)
            delete m_list->takeItem(i);
    }
    updateHeader();
}

QListWidgetItem *RecentCaptures::createItem(const QString &path)
{
    auto *item = new QListWidgetItem;
    item->setData(kPathRole, path);
    item->setData(kLoadedRole, false);
    // The names from the pattern look alike when shortened; the time of
    // the capture tells them apart (today: the time only).
    const QDateTime time = QFileInfo(path).lastModified();
    const QLocale locale;
    item->setText(time.date() == QDate::currentDate() ? locale.toString(time.time(), QStringLiteral("HH:mm:ss"))
                                                      : locale.toString(time, QLocale::ShortFormat));
    item->setToolTip(tr("%1\nKlicken, um im Editor zu öffnen").arg(QDir::toNativeSeparators(path)));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    // Fixed: the style sheet's item padding would otherwise shrink the
    // item to one line of text and cut the thumbnail.
    item->setSizeHint(QSize(kThumbWidth + 12, kThumbHeight + 30));
    return item;
}

void RecentCaptures::loadNextThumbnail()
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (item->data(kLoadedRole).toBool())
            continue;
        // Read at the size of the thumbnail where the format allows it.
        QImageReader reader(item->data(kPathRole).toString());
        const qreal ratio = devicePixelRatioF();
        const QSize full = reader.size();
        if (full.isValid())
            reader.setScaledSize(full.scaled(QSize(kThumbWidth, kThumbHeight) * ratio, Qt::KeepAspectRatio));
        setThumbnail(item, reader.read());
        return; // one per turn of the event loop
    }
    m_loader->stop();
}

void RecentCaptures::setThumbnail(QListWidgetItem *item, const QImage &image)
{
    item->setData(kLoadedRole, true);
    if (image.isNull())
        return;
    // Every thumbnail the same size, the picture centred in it, so the row
    // stays even; drawn at the screen's pixel density.
    const qreal ratio = devicePixelRatioF();
    QPixmap thumb(QSize(kThumbWidth, kThumbHeight) * ratio);
    thumb.setDevicePixelRatio(ratio);
    thumb.fill(Qt::transparent);
    const QSize size = image.size().scaled(QSize(kThumbWidth, kThumbHeight), Qt::KeepAspectRatio);
    const QRect target((kThumbWidth - size.width()) / 2, (kThumbHeight - size.height()) / 2, size.width(),
                       size.height());
    QPainter painter(&thumb);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, image);
    // A thin outline, so a white capture stands out from the panel.
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(target).adjusted(0.5, 0.5, -0.5, -0.5));
    painter.end();
    item->setIcon(QIcon(thumb));
}

void RecentCaptures::open(QListWidgetItem *item)
{
    if (!item)
        return;
    emit openRequested(item->data(kPathRole).toString());
    if (!m_pinned)
        setExpanded(false);
}

void RecentCaptures::showMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    if (!item)
        return;
    const QString path = item->data(kPathRole).toString();
    QMenu menu(this);
    menu.addAction(tr("Im Editor öffnen"), this, [this, item] { open(item); });
    menu.addAction(tr("Ordner öffnen"), this, [path] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    menu.addSeparator();
    menu.addAction(tr("Aus der Liste entfernen"), this, [this, path] {
        removePath(path);
        emit pathsChanged(paths());
    });
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

void RecentCaptures::setExpanded(bool expanded)
{
    // Pinned, it stays open.
    if (m_pinned)
        expanded = true;
    m_expanded = expanded;
    m_list->setVisible(expanded);
    if (expanded)
        m_loader->start();
    updateHeader();
}

void RecentCaptures::setPinned(bool pinned)
{
    m_pinned = pinned;
    {
        const QSignalBlocker blocker(m_pin);
        m_pin->setChecked(pinned);
    }
    m_pin->setToolTip(pinned ? tr("Lösen: Die Liste schliesst sich nach dem Öffnen einer Aufnahme")
                             : tr("Anheften: Die Liste bleibt offen"));
    if (pinned)
        setExpanded(true);
    updateHeader();
}

void RecentCaptures::applyTheme(const Theme &theme)
{
    m_iconColor = theme.muted;
    m_pin->setIcon(Icons::toggleIcon(QStringLiteral("pin"), theme.muted, theme.softText));
    m_clear->setIcon(Icons::icon(QStringLiteral("clear"), theme.muted));
    updateHeader();
}

void RecentCaptures::updateHeader()
{
    const int count = m_list->count();
    m_toggle->setText(tr("Letzte Aufnahmen · %1").arg(count));
    m_toggle->setToolTip(m_expanded ? tr("Liste schliessen") : tr("Liste öffnen"));
    if (m_iconColor.isValid())
        m_toggle->setIcon(Icons::icon(m_expanded ? QStringLiteral("chevron-down") : QStringLiteral("chevron-up"),
                                      m_iconColor));
    m_clear->setEnabled(count > 0);
    setVisible(count > 0);
}
