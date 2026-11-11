#pragma once

#include "capture/CountdownOptions.h"

#include <QTimer>
#include <QWidget>

// Countdown of a delayed capture (F-04, D-025) where the app draws the
// overlay itself (Windows, Wayland without GNOME; later macOS): a small
// top-most display on the primary monitor (in the middle or a corner, as
// set) that never takes the focus, so the user can keep working (open a
// menu, hover) until it runs out. Escape cancels, and a click on it unless
// switched off; then clicks go through it. On Windows Escape is a global
// hotkey while it runs, because the key goes to the window that has the
// focus. Wayland has no global keys for apps: there the click always
// cancels, else nothing could.
class Countdown : public QWidget
{
    Q_OBJECT

public:
    explicit Countdown(QWidget *parent = nullptr);
    ~Countdown() override;

    // Before start(): position and click (from the settings).
    void setOptions(const CountdownOptions &options);
    const CountdownOptions &options() const { return m_options; }
    // Whether Escape reaches the countdown while another app has the focus.
    static bool hasGlobalEscape();

    void start(int seconds);
    void cancel();
    int secondsLeft() const { return m_left; }

    // For tests: shorter ticks than a second.
    void setTickInterval(int milliseconds) { m_tick.setInterval(milliseconds); }

signals:
    // The countdown ran out and the display is gone from the screen.
    void finished();
    void cancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void tick();
    void stop();

    CountdownOptions m_options;
    QTimer m_tick;
    int m_left = 0;
    bool m_running = false;
    bool m_shown = false;
};
