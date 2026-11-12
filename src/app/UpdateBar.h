#pragma once

#include <QFrame>

class QLabel;

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

signals:
    void openRequested();
    void dismissed(const QString &version);

private:
    QString m_version;
    QLabel *m_text = nullptr;
};
