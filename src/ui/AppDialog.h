#pragma once

#include <QList>
#include <QPointer>
#include <QString>

class QCheckBox;
class QFrame;
class QLabel;
class QPushButton;
class QWidget;

// The app's own message and question dialog (design mockup, "Dialoge"):
// a card over the dimmed window it belongs to, without a title bar of its
// own. When that window is not on the screen (the app runs in the tray, or
// a capture from the command line), the card comes in a small frameless
// window instead.
//
// Use it like QMessageBox:
//
//     AppDialog dialog(this, AppDialog::Kind::Question, tr("Video verwerfen?"),
//                      tr("Die Aufnahme ist nicht gespeichert."));
//     QPushButton *discard = dialog.addButton(tr("Verwerfen"), AppDialog::Role::Destructive);
//     dialog.addButton(tr("Abbrechen"), AppDialog::Role::Cancel);
//     if (dialog.exec() == discard) ...
//
// exec() waits (in its own event loop) until a button is clicked. Escape
// counts as the Cancel button, Enter as the default one. While the card is
// open, the window's keyboard shortcuts and its other widgets do nothing.
class AppDialog
{
public:
    enum class Kind { Question, Information, Warning, Error };
    enum class Role {
        Primary,     // the blue button: the suggested action
        Secondary,   // a plain button
        Destructive, // the red button: an action that throws something away
        Cancel,      // a plain button that Escape also presses
    };

    // `title` is the bold first line (a question, or what happened),
    // `text` the explanation below it; both may be empty but one.
    AppDialog(QWidget *parent, Kind kind, const QString &title, const QString &text = {});
    ~AppDialog();
    AppDialog(const AppDialog &) = delete;
    AppDialog &operator=(const AppDialog &) = delete;

    // Buttons appear left to right in the order they are added, at the
    // card's right edge.
    QPushButton *addButton(const QString &text, Role role);
    // Enter presses it, and it has the focus. Without a call: the Cancel
    // button when there is a destructive one (Enter never throws anything
    // away), otherwise the primary one, otherwise the last one.
    void setDefaultButton(QPushButton *button);
    // An option under the text, e.g. "In Zukunft nicht mehr nachfragen".
    QCheckBox *setCheckBox(const QString &text);

    // Shows the dialog and returns the clicked button (the Cancel button
    // for Escape; nullptr if there is none and the dialog was closed).
    QPushButton *exec();

    // A dialog of this kind is open somewhere (global hotkeys wait).
    static bool isOpen();

    // One-button messages ("OK").
    static void information(QWidget *parent, const QString &title, const QString &text);
    static void warning(QWidget *parent, const QString &title, const QString &text);
    static void error(QWidget *parent, const QString &title, const QString &text);

private:
    QPushButton *chooseDefault() const;

    QPointer<QWidget> m_parent;
    QFrame *m_card = nullptr; // owned by m_host or by this object until shown
    QLabel *m_icon = nullptr;
    QWidget *m_buttonRow = nullptr;
    QCheckBox *m_checkBox = nullptr;
    QList<std::pair<QPushButton *, Role>> m_buttons;
    QPushButton *m_default = nullptr;
};
