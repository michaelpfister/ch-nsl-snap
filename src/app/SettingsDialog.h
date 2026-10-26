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
    // The styles for new objects were reset to the defaults (F-22).
    void stylesReset();

private:
    QCheckBox *m_advanced = nullptr;
};
