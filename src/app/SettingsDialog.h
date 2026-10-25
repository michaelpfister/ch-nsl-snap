#pragma once

#include <QDialog>

class QCheckBox;

// "Einstellungen" (F2, F-46). Changes apply immediately.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

signals:
    void advancedModeChanged(bool enabled);

private:
    QCheckBox *m_advanced = nullptr;
};
