#pragma once

#include <QTimer>
#include <QWidget>

// Countdown of a delayed capture (F-04, D-025) where the app draws the
// overlay itself (Windows, Wayland without GNOME; later macOS): a small
// top-most display in the middle of the primary monitor that never takes
// the focus, so the user can keep working (open a menu, hover) until it
// runs out. A click on it or Escape cancels; on Windows Escape is a global
// hotkey while it runs, because the key goes to the window that has the
// focus. Wayland has no global keys for apps: there only the click works.
class Countdown : public QWidget
{
    Q_OBJECT

public:
    explicit Countdown(QWidget *parent = nullptr);
    ~Countdown() override;

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

    QTimer m_tick;
    int m_left = 0;
    bool m_running = false;
    bool m_shown = false;
};
