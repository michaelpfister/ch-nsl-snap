#include "app/UpdateBar.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

UpdateBar::UpdateBar(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("UpdateBar"));
    // Colours from the style sheet (Theme): the soft accent tint.
    setAttribute(Qt::WA_StyledBackground);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 8, 8, 8);
    layout->setSpacing(12);

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/dialog-information.svg")).pixmap(QSize(18, 18), devicePixelRatioF()));
    layout->addWidget(icon);

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

void UpdateBar::setVersion(const QString &version)
{
    m_version = version;
    setVisible(!version.isEmpty());
    if (!version.isEmpty())
        m_text->setText(tr("<b>Snapslash %1 ist verfügbar.</b> Die Einstellungen bleiben beim Update erhalten.")
                            .arg(version.toHtmlEscaped()));
}
