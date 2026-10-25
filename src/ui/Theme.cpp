#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QList>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

Theme Theme::light()
{
    Theme t;
    t.dark = false;
    t.bg = QColor(0xF3F4F6);
    t.panel = QColor(0xFFFFFF);
    t.text = QColor(0x16191D);
    t.muted = QColor(0x59616C);
    t.border = QColor(0xE2E5EA);
    t.canvas = QColor(0xE7E9ED);
    t.hover = QColor(0xEEF1F5);
    t.accent = QColor(0x2563EB);
    t.soft = QColor(0xE3ECFD);
    t.softText = QColor(0x1D4ED8);
    return t;
}

Theme Theme::darkTheme()
{
    Theme t;
    t.dark = true;
    t.bg = QColor(0x15171A);
    t.panel = QColor(0x1D2024);
    t.text = QColor(0xE9ECEF);
    t.muted = QColor(0xA2A9B3);
    t.border = QColor(0x2D3238);
    t.canvas = QColor(0x0F1113);
    t.hover = QColor(0x272B31);
    t.accent = QColor(0x2563EB);
    t.soft = QColor(0x1E2A44);
    t.softText = QColor(0x9DBDFF);
    return t;
}

Theme Theme::current()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? darkTheme() : light();
}

QString Theme::styleSheet() const
{
    // Object names (#TopBar, #ToolRail, …) and the "role" property select
    // the widgets; see MainWindow and the panels. @name are colour tokens.
    QString qss = QStringLiteral(R"(
QMainWindow, #Central { background: @bg; }
QWidget { color: @text; }

#TopBar, #ToolRail, #PropertiesPanel, QStatusBar { background: @panel; }
#TopBar { border-bottom: 1px solid @border; }
#ToolRail { border-right: 1px solid @border; }
#PropertiesPanel { border-left: 1px solid @border; }
QStatusBar { border-top: 1px solid @border; color: @muted; font-size: 12px; }
QStatusBar QLabel { color: @muted; font-size: 12px; padding: 0 4px; }
QStatusBar::item { border: none; }

#AppName { font-weight: 600; font-size: 14px; }
QLabel[role="muted"] { color: @muted; }
QLabel[role="section"] { color: @muted; font-size: 12px; font-weight: 500; }
QLabel[role="panelTitle"] { font-size: 14px; font-weight: 600; }
QLabel[role="hint"] { color: @muted; font-size: 12px; }
QFrame[role="separator"] { background: @border; max-height: 1px; min-height: 1px; border: none; }

/* Segmented controls: a track with buttons, the checked one raised. */
QFrame[role="segment"] { background: @hover; border-radius: 10px; }
QFrame[role="segment"] QToolButton, QFrame[role="segment"] QPushButton {
    background: transparent; border: none; border-radius: 8px;
    color: @muted; padding: 0 12px; min-height: 30px; font-weight: 500;
}
QFrame[role="segment"] QToolButton:hover, QFrame[role="segment"] QPushButton:hover { color: @text; }
QFrame[role="segment"] QToolButton::menu-indicator { image: none; width: 0; }
QMenu { background: @panel; color: @text; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 6px; }
QMenu::item:selected { background: @soft; }
QMenu::separator { height: 1px; background: @border; margin: 4px 6px; }
QFrame[role="segment"] QToolButton:checked, QFrame[role="segment"] QPushButton:checked {
    background: @panel; color: @text; border: 1px solid @border;
}

/* Plain icon buttons (undo, redo). */
QToolButton[role="icon"] { background: transparent; border: none; border-radius: 8px; padding: 6px; }
QToolButton[role="icon"]:hover { background: @hover; }
QToolButton[role="icon"]:checked { background: @soft; }

/* Tool rail. */
#ToolRail QToolButton { background: transparent; border: none; border-radius: 10px; padding: 0; }
#ToolRail QToolButton:hover { background: @hover; }
#ToolRail QToolButton:checked { background: @soft; }

/* Buttons. */
QPushButton[role="secondary"] {
    background: @panel; color: @text; border: 1px solid @border; border-radius: 8px;
    padding: 0 12px; min-height: 32px; font-weight: 500;
}
QPushButton[role="secondary"]:hover { background: @hover; }
QPushButton[role="primary"] {
    background: @accent; color: #FFFFFF; border: none; border-radius: 8px;
    padding: 0 14px; min-height: 34px; font-weight: 600;
}
QPushButton[role="primary"]:hover { background: @accentHover; }
QPushButton:disabled, QToolButton:disabled { color: @muted; }
QPushButton[role="primary"]:disabled { background: @border; color: @muted; }

/* Inputs. */
QComboBox {
    background: @panel; color: @text; border: 1px solid @border; border-radius: 8px;
    padding: 0 8px; min-height: 32px;
}
QComboBox:hover { border-color: @muted; }
QComboBox:disabled { color: @muted; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(@chevron); width: 14px; height: 14px; }
QComboBox QAbstractItemView { background: @panel; color: @text; border: 1px solid @border; selection-background-color: @soft; selection-color: @text; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px; border: 1.5px solid @muted; background: @panel; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; image: url(:/icons/check-white.svg); }
QSlider::groove:horizontal { height: 4px; background: @border; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
QSlider::handle:horizontal { background: #FFFFFF; border: 2px solid @accent; width: 12px; height: 12px; margin: -6px 0; border-radius: 8px; }

QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item { padding: 6px 8px; border-radius: 7px; }
QListWidget::item:selected { background: @soft; color: @text; }

QToolTip { background: @panel; color: @text; border: 1px solid @border; padding: 4px 6px; }
QScrollBar:vertical, QScrollBar:horizontal { background: transparent; width: 10px; height: 10px; }
QScrollBar::handle { background: @border; border-radius: 5px; min-height: 30px; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
)");
    const QList<std::pair<QString, QColor>> tokens = {
        // Longest names first, so "@accentHover" is not hit by "@accent".
        {QStringLiteral("@accentHover"), accent.darker(112)},
        {QStringLiteral("@accent"), accent},
        {QStringLiteral("@border"), border},
        {QStringLiteral("@canvas"), canvas},
        {QStringLiteral("@hover"), hover},
        {QStringLiteral("@muted"), muted},
        {QStringLiteral("@panel"), panel},
        {QStringLiteral("@soft"), soft},
        {QStringLiteral("@text"), text},
        {QStringLiteral("@bg"), bg},
    };
    for (const auto &[name, color] : tokens)
        qss.replace(name, color.name());
    qss.replace(QStringLiteral("@chevron"),
                dark ? QStringLiteral(":/icons/chevron-down-dark.svg") : QStringLiteral(":/icons/chevron-down-light.svg"));
    return qss;
}

void Theme::apply(QApplication &app) const
{
    // Fusion draws the same everywhere; the style sheet does the rest.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, panel);
    p.setColor(QPalette::AlternateBase, hover);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, panel);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, QColor(Qt::white));
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Dark, canvas);
    p.setColor(QPalette::Mid, border);
    p.setColor(QPalette::ToolTipBase, panel);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Disabled, QPalette::Text, muted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, muted);
    app.setPalette(p);

    // Inter as in the design if installed, otherwise the system UI font.
    QFont font = QApplication::font();
    font.setFamilies({QStringLiteral("Inter"), font.family()});
    font.setPixelSize(13);
    app.setFont(font);

    app.setStyleSheet(styleSheet());
}
