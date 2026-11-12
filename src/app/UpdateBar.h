#pragma once

#include <QFrame>

class LogoMarkWidget;
class QLabel;
class QVariantAnimation;

// The note at the bottom of the window that a newer version is available
// (F-49): a tinted bar with the version, a button that opens the update
// dialog, and a close button that hides it for this version.
class UpdateBar : public QFrame
{
    Q_OBJECT

public:
    explicit UpdateBar(QWidget *parent = nullptr);

    // Shows the bar for `version` ("0.3.1"); an empty string hides it.
    void setVersion(const QString &version);
    QString version() const { return m_version; }
    // Draws attention once (D-043): the bar grows a little, the mark turns
    // into an arrow pointing at the button and nudges twice, then all goes
    // back. Only while the bar is on the screen.
    void announce();

signals:
    void openRequested();
    void dismissed(const QString &version);

private:
    QString m_version;
    void setMarkSize(int side);

    QLabel *m_text = nullptr;
    LogoMarkWidget *m_mark = nullptr;
    QVariantAnimation *m_grow = nullptr;
    int m_restHeight = 0; // the height before announce(), 0 at rest
};
