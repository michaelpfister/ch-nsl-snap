#pragma once

#include <QImage>
#include <QStringList>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QTimer;
class QToolButton;
struct Theme;

// "Letzte Aufnahmen" (F-52): a strip at the bottom of the main window with
// the captures saved last, as thumbnails. The header opens and closes it;
// pinned, it stays open, otherwise it closes again after a capture was
// opened from it. A click on a thumbnail asks for it to be opened in the
// editor (openRequested). Hidden while the list is empty.
class RecentCaptures : public QWidget
{
    Q_OBJECT

public:
    explicit RecentCaptures(QWidget *parent = nullptr);

    // The files, newest first. Files that no longer exist are left out.
    void setPaths(const QStringList &paths);
    QStringList paths() const;
    // A new capture, first in the list; `image` gives the thumbnail at
    // once (else it is read from the file).
    void addCapture(const QString &path, const QImage &image = QImage());
    void removePath(const QString &path);

    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);
    bool isPinned() const { return m_pinned; }
    void setPinned(bool pinned);

    void applyTheme(const Theme &theme);

    // Size of the thumbnails (logical pixels).
    static constexpr int kThumbWidth = 144;
    static constexpr int kThumbHeight = 90;

signals:
    void openRequested(const QString &path);
    // The list changed (added, removed, cleared): to be remembered.
    void pathsChanged(const QStringList &paths);
    void pinnedChanged(bool pinned);

private:
    QListWidgetItem *createItem(const QString &path);
    // Thumbnails are read from the files one at a time while the strip is
    // open, so a long list does not hold up the start.
    void loadNextThumbnail();
    void setThumbnail(QListWidgetItem *item, const QImage &image);
    void open(QListWidgetItem *item);
    void showMenu(const QPoint &pos);
    void updateHeader();

    QToolButton *m_toggle = nullptr;
    QToolButton *m_pin = nullptr;
    QToolButton *m_clear = nullptr;
    QListWidget *m_list = nullptr;
    QTimer *m_loader = nullptr;
    bool m_expanded = false;
    bool m_pinned = false;
    QColor m_iconColor;
};
