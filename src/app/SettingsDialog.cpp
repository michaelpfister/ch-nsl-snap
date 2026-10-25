#include "app/SettingsDialog.h"

#include "app/Settings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(6);

    m_advanced = new QCheckBox(tr("Erweiterter Modus"), this);
    m_advanced->setChecked(Settings::advancedMode());
    layout->addWidget(m_advanced);

    auto *hint = new QLabel(tr("Zeigt zusätzliche Regler im Eigenschaften-Panel, "
                               "zum Beispiel für die Stärke des Abdunkelns und der Unschärfe."), this);
    hint->setProperty("role", QStringLiteral("hint"));
    hint->setWordWrap(true);
    hint->setContentsMargins(26, 0, 0, 0); // aligned with the checkbox text
    layout->addWidget(hint);
    layout->addSpacing(16);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Schliessen"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_advanced, &QCheckBox::toggled, this, [this](bool enabled) {
        Settings::setAdvancedMode(enabled);
        emit advancedModeChanged(enabled);
    });
}
