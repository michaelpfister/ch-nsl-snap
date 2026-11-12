#include "ui/AppDialog.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QEventLoop>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

int openDialogs = 0;
constexpr int kCardWidth = 420;

QString iconFor(AppDialog::Kind kind)
{
    switch (kind) {
    case AppDialog::Kind::Question: return QStringLiteral(":/icons/dialog-question.svg");
    case AppDialog::Kind::Information: return QStringLiteral(":/icons/dialog-information.svg");
    case AppDialog::Kind::Warning: return QStringLiteral(":/icons/dialog-warning.svg");
    case AppDialog::Kind::Error: return QStringLiteral(":/icons/dialog-error.svg");
    }
    return {};
}

QString roleName(AppDialog::Role role)
{
    switch (role) {
    case AppDialog::Role::Primary: return QStringLiteral("primary");
    case AppDialog::Role::Destructive: return QStringLiteral("destructive");
    case AppDialog::Role::Secondary:
    case AppDialog::Role::Cancel: return QStringLiteral("secondary");
    }
    return {};
}

// The card. Keeps Tab among its own buttons and handles Enter and Escape:
// outside a QDialog, Qt's buttons do not react to Enter by themselves.
class Card : public QFrame
{
public:
    std::function<void()> onEnter;
    std::function<void()> onEscape;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (onEnter)
                onEnter();
            return;
        case Qt::Key_Escape:
            if (onEscape)
                onEscape();
            return;
        default:
            // Nothing else reaches the window behind the card.
            event->accept();
        }
    }

    bool focusNextPrevChild(bool next) override
    {
        QList<QWidget *> chain;
        for (QWidget *child : findChildren<QWidget *>()) {
            if (child->isVisible() && child->isEnabled() && (child->focusPolicy() & Qt::TabFocus))
                chain.append(child);
        }
        if (chain.isEmpty())
            return false;
        // In reading order: top to bottom, then left to right.
        std::sort(chain.begin(), chain.end(), [this](QWidget *a, QWidget *b) {
            const QPoint pa = a->mapTo(this, QPoint());
            const QPoint pb = b->mapTo(this, QPoint());
            return pa.y() != pb.y() ? pa.y() < pb.y() : pa.x() < pb.x();
        });
        const qsizetype at = chain.indexOf(QApplication::focusWidget());
        const qsizetype count = chain.size();
        const qsizetype to = at < 0 ? 0 : (at + (next ? 1 : count - 1)) % count;
        chain.at(to)->setFocus(next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
        return true;
    }
};

// The dimmed layer over the window. It takes every click and key, so the
// window behind it cannot be used while the question is open, and it
// follows the window's size.
class Scrim : public QWidget
{
public:
    explicit Scrim(QWidget *window)
        : QWidget(window)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        m_color = Theme::current().dark ? QColor(0, 0, 0, 140) : QColor(15, 17, 19, 102);
        setGeometry(window->rect());
        window->installEventFilter(this);
    }

protected:
    void paintEvent(QPaintEvent *) override { QPainter(this).fillRect(rect(), m_color); }
    // A click beside the card does nothing: the question needs an answer.
    void mousePressEvent(QMouseEvent *event) override { event->accept(); }
    void mouseReleaseEvent(QMouseEvent *event) override { event->accept(); }
    void mouseDoubleClickEvent(QMouseEvent *event) override { event->accept(); }
    void wheelEvent(QWheelEvent *event) override { event->accept(); }
    void keyPressEvent(QKeyEvent *event) override { event->accept(); }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        return QWidget::eventFilter(watched, event);
    }

private:
    QColor m_color;
};

// The small frameless window for when no window of the app is visible. A
// transparent margin leaves room for the card's shadow.
class FrameWindow : public QWidget
{
public:
    std::function<void()> onClose;

    FrameWindow()
        : QWidget(nullptr, Qt::Dialog | Qt::FramelessWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowModality(Qt::ApplicationModal);
    }

protected:
    // Closed by the desktop (Alt+F4): like Escape.
    void closeEvent(QCloseEvent *event) override
    {
        event->ignore();
        if (onClose)
            onClose();
    }
};

// While a card is open, the window's shortcuts (Ctrl+S, Delete, ...) must
// not act behind it: their events are dropped unless they belong to the
// card itself.
class ShortcutBlocker : public QObject
{
public:
    explicit ShortcutBlocker(QWidget *card)
        : m_card(card)
    {
        qApp->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Shortcut) {
            auto *widget = qobject_cast<QWidget *>(watched);
            if (!widget || (widget != m_card && !m_card->isAncestorOf(widget)))
                return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *m_card;
};

} // namespace

AppDialog::AppDialog(QWidget *parent, Kind kind, const QString &title, const QString &text)
    : m_parent(parent)
{
    auto *card = new Card;
    m_card = card;
    m_card->setObjectName(QStringLiteral("DialogCard"));
    // The style sheet's colours and corners apply to a plain QFrame only
    // with a styled background.
    m_card->setAttribute(Qt::WA_StyledBackground);
    m_card->setFocusPolicy(Qt::NoFocus);
    // Screen readers: the card is the dialog, named by its title.
    m_card->setAccessibleName(title.isEmpty() ? text : title);
    m_card->setAccessibleDescription(title.isEmpty() ? QString() : text);

    auto *layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(20);

    // Icon left, title and text right of it.
    auto *top = new QHBoxLayout;
    top->setSpacing(14);
    m_icon = new QLabel(m_card);
    m_icon->setPixmap(QIcon(iconFor(kind)).pixmap(QSize(32, 32), m_card->devicePixelRatioF()));
    m_icon->setFixedSize(32, 32);
    top->addWidget(m_icon, 0, Qt::AlignTop);
    auto *texts = new QVBoxLayout;
    texts->setSpacing(6);
    texts->setContentsMargins(0, 4, 0, 0);
    if (!title.isEmpty()) {
        auto *titleLabel = new QLabel(title, m_card);
        titleLabel->setProperty("role", QStringLiteral("dialogTitle"));
        titleLabel->setWordWrap(true);
        titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        texts->addWidget(titleLabel);
    }
    if (!text.isEmpty()) {
        auto *textLabel = new QLabel(text, m_card);
        textLabel->setProperty("role", QStringLiteral("dialogText"));
        textLabel->setWordWrap(true);
        // Error texts name paths and causes: copyable.
        textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        texts->addWidget(textLabel);
    }
    top->addLayout(texts, 1);
    layout->addLayout(top);

    m_buttonRow = new QWidget(m_card);
    auto *buttons = new QHBoxLayout(m_buttonRow);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(8);
    buttons->addStretch(1);
    layout->addWidget(m_buttonRow);

    // The card's shadow, as in the mockup.
    auto *shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 16);
    shadow->setColor(QColor(0, 0, 0, 70));
    m_card->setGraphicsEffect(shadow);
}

AppDialog::~AppDialog()
{
    // Not shown, or shown and already handed to its host for deletion.
    delete m_card;
}

QPushButton *AppDialog::addButton(const QString &text, Role role)
{
    auto *button = new QPushButton(text, m_buttonRow);
    button->setProperty("role", roleName(role));
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAutoDefault(false);
    m_buttonRow->layout()->addWidget(button);
    m_buttons.append({button, role});
    return button;
}

void AppDialog::setDefaultButton(QPushButton *button)
{
    m_default = button;
}

QCheckBox *AppDialog::setCheckBox(const QString &text)
{
    if (!m_checkBox) {
        m_checkBox = new QCheckBox(text, m_card);
        m_checkBox->setFocusPolicy(Qt::StrongFocus);
        // Under the text, aligned with it (icon 32 px + gap 14 px).
        auto *layout = static_cast<QVBoxLayout *>(m_card->layout());
        auto *row = new QHBoxLayout;
        row->setContentsMargins(46, 0, 0, 0);
        row->addWidget(m_checkBox);
        layout->insertLayout(1, row);
    }
    m_checkBox->setText(text);
    return m_checkBox;
}

QPushButton *AppDialog::chooseDefault() const
{
    if (m_default)
        return m_default;
    QPushButton *cancel = nullptr;
    QPushButton *primary = nullptr;
    bool destructive = false;
    for (const auto &[button, role] : m_buttons) {
        if (role == Role::Cancel)
            cancel = button;
        else if (role == Role::Primary)
            primary = button;
        else if (role == Role::Destructive)
            destructive = true;
    }
    if (destructive && cancel)
        return cancel;
    if (primary)
        return primary;
    return m_buttons.isEmpty() ? nullptr : m_buttons.last().first;
}

QPushButton *AppDialog::exec()
{
    QPushButton *cancel = nullptr;
    for (const auto &[button, role] : m_buttons) {
        if (role == Role::Cancel)
            cancel = button;
    }
    QPushButton *defaultButton = chooseDefault();
    QPushButton *clicked = nullptr;
    QEventLoop loop;
    const auto finish = [&](QPushButton *button) {
        clicked = button;
        loop.quit();
    };
    for (const auto &[button, role] : m_buttons)
        QObject::connect(button, &QPushButton::clicked, &loop, [&finish, button = button] { finish(button); });
    auto *card = static_cast<Card *>(m_card);
    card->onEnter = [&] {
        // Enter presses the button with the focus, else the default one.
        auto *focused = qobject_cast<QPushButton *>(QApplication::focusWidget());
        if (focused && m_card->isAncestorOf(focused))
            finish(focused);
        else if (defaultButton)
            finish(defaultButton);
    };
    card->onEscape = [&] {
        if (cancel)
            finish(cancel);
        else if (m_buttons.size() == 1)
            finish(m_buttons.first().first); // a message: Escape closes it
    };

    const QPointer<QWidget> previousFocus = QApplication::focusWidget();
    QWidget *window = m_parent ? m_parent->window() : nullptr;
    const bool inWindow = window && window->isVisible() && !window->isMinimized();
    QWidget *host = nullptr;
    if (inWindow) {
        auto *scrim = new Scrim(window);
        auto *center = new QVBoxLayout(scrim);
        center->setContentsMargins(16, 16, 16, 16);
        center->addWidget(m_card, 0, Qt::AlignCenter);
        m_card->setFixedWidth(qMin(kCardWidth, window->width() - 32));
        scrim->show();
        scrim->raise();
        host = scrim;
    } else {
        auto *frame = new FrameWindow;
        frame->onClose = card->onEscape;
        auto *margin = new QVBoxLayout(frame);
        // Room for the shadow (blur 48, offset 16).
        margin->setContentsMargins(40, 24, 40, 56);
        margin->addWidget(m_card);
        m_card->setFixedWidth(kCardWidth);
        frame->setWindowTitle(QApplication::applicationDisplayName());
        frame->show();
        frame->activateWindow();
        host = frame;
    }
    // From now on the host deletes the card.
    QFrame *ownCard = m_card;
    m_card = nullptr;
    const QPointer<QWidget> hostGuard = host;
    // The window behind it closed (the app quits): stop waiting.
    QObject::connect(host, &QObject::destroyed, &loop, &QEventLoop::quit);

    if (defaultButton)
        defaultButton->setFocus(Qt::OtherFocusReason);
    else
        ownCard->setFocus(Qt::OtherFocusReason);

    ShortcutBlocker blocker(ownCard);
    ++openDialogs;
    loop.exec();
    --openDialogs;

    if (hostGuard) {
        hostGuard->hide();
        // Later, so the returned button pointer stays valid for the caller's
        // comparison and a checkbox can still be read.
        hostGuard->deleteLater();
    }
    if (previousFocus)
        previousFocus->setFocus(Qt::OtherFocusReason);
    return clicked;
}

bool AppDialog::isOpen()
{
    return openDialogs > 0;
}

void AppDialog::information(QWidget *parent, const QString &title, const QString &text)
{
    AppDialog dialog(parent, Kind::Information, title, text);
    dialog.addButton(QApplication::translate("AppDialog", "OK"), Role::Primary);
    dialog.exec();
}

void AppDialog::warning(QWidget *parent, const QString &title, const QString &text)
{
    AppDialog dialog(parent, Kind::Warning, title, text);
    dialog.addButton(QApplication::translate("AppDialog", "OK"), Role::Primary);
    dialog.exec();
}

void AppDialog::error(QWidget *parent, const QString &title, const QString &text)
{
    AppDialog dialog(parent, Kind::Error, title, text);
    dialog.addButton(QApplication::translate("AppDialog", "OK"), Role::Primary);
    dialog.exec();
}
