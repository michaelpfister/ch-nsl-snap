#include "app/UpdateBar.h"

#include "ui/LogoMark.h"
#include "ui/Motion.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVariantAnimation>

namespace {

constexpr int kMarkSize = 20;   // the mark at rest
constexpr int kMarkGrown = 28;  // while announcing
constexpr int kGrowBy = 16;     // px the bar grows

} // namespace

UpdateBar::UpdateBar(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("UpdateBar"));
    // Colours from the style sheet (Theme): the soft accent tint.
    setAttribute(Qt::WA_StyledBackground);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 8, 8, 8);
    layout->setSpacing(12);

    // The app's mark, in the bar's blue (style sheet: #UpdateBarMark).
    m_mark = new LogoMarkWidget(this);
    m_mark->setObjectName(QStringLiteral("UpdateBarMark"));
    setMarkSize(kMarkSize);
    layout->addWidget(m_mark);

    // Growing for announce(): the extra height, 0 → 1 → 0.
    m_grow = new QVariantAnimation(this);
    m_grow->setDuration(260);
    m_grow->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_grow, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const qreal t = value.toReal();
        setFixedHeight(m_restHeight + qRound(kGrowBy * t));
        setMarkSize(kMarkSize + qRound((kMarkGrown - kMarkSize) * t));
    });
    connect(m_mark->animator(), &LogoAnimator::finished, this, [this] {
        if (m_restHeight <= 0)
            return;
        m_grow->setStartValue(1.0);
        m_grow->setEndValue(0.0);
        m_grow->start();
    });
    connect(m_grow, &QVariantAnimation::finished, this, [this] {
        if (m_grow->endValue().toReal() > 0)
            return;
        // Back at rest: the layout decides the height again.
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        m_restHeight = 0;
    });

    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("UpdateBarText"));
    layout->addWidget(m_text, 1);

    auto *open = new QPushButton(tr("Anzeigen und installieren"), this);
    open->setProperty("role", QStringLiteral("primary"));
    open->setObjectName(QStringLiteral("UpdateBarOpen"));
    connect(open, &QPushButton::clicked, this, &UpdateBar::openRequested);
    layout->addWidget(open);

    auto *close = new QToolButton(this);
    close->setObjectName(QStringLiteral("UpdateBarClose"));
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Für diese Version ausblenden"));
    close->setAccessibleName(tr("Für diese Version ausblenden"));
    connect(close, &QToolButton::clicked, this, [this] {
        const QString version = m_version;
        setVersion({});
        emit dismissed(version);
    });
    layout->addWidget(close);

    setVisible(false);
}

void UpdateBar::setMarkSize(int side)
{
    // Room on the right: the arrow's nudge reaches past the square.
    m_mark->setFixedSize(side + side / 3, side);
}

void UpdateBar::announce()
{
    if (!isVisible() || m_restHeight > 0)
        return;
    m_restHeight = height();
    if (Motion::reduced())
        return; // no movement; the bar itself is the note
    m_grow->setStartValue(0.0);
    m_grow->setEndValue(1.0);
    m_grow->start();
    m_mark->animator()->play(LogoAnimator::updateSteps());
}

void UpdateBar::setVersion(const QString &version)
{
    m_version = version;
    setVisible(!version.isEmpty());
    if (!version.isEmpty())
        m_text->setText(tr("<b>Snapslash %1 ist verfügbar.</b> Die Einstellungen bleiben beim Update erhalten.")
                            .arg(version.toHtmlEscaped()));
}
