#pragma once

#include "recording/RecordingBackend.h"

#include <QRect>
#include <QVariantMap>

#include <memory>

class GnomeShellBackend;
class PipeWireStream;

// Videos on GNOME (F-50, D-039, step 4). The Snapslash Shell extension
// freezes the screen and lets the user pick, as for an image, but answers
// with what was picked instead of an image: the window's id or the screen's
// rectangle (API version 7). The app then records it itself with Mutter's
// screen cast (org.gnome.Mutter.ScreenCast: RecordWindow or RecordArea,
// with the pointer) and reads the frames from PipeWire (PipeWireStream).
//
// GNOME shows its own screen-sharing indicator in the top bar while the
// screen cast runs; in a monitor recording it is in the video, as is the
// app's indicator (Q-31).
class GnomeRecordingBackend : public RecordingBackend
{
    Q_OBJECT

public:
    explicit GnomeRecordingBackend(QObject *parent = nullptr);
    ~GnomeRecordingBackend() override;

    bool isAvailable() const override;
    QString unavailableReason() const override;
    bool supportsWindow() const override { return true; }

    void pick(Source source) override;
    void startStream() override;
    void requestFrame() override;
    void stopStream() override;

    // What the extension's reply says was picked; public for the tests.
    struct Pick
    {
        bool window = false;
        quint64 windowId = 0; // Mutter's window id (MetaWindow::get_id)
        QRect area;           // screen: logical px, for RecordArea
        QSize size;           // physical px
        QString title;
    };
    static Pick pickFromReply(const QVariantMap &info);

private slots:
    // Mutter's signals; string-based slots, as QtDBus connects them by name.
    void onPipeWireStreamAdded(uint nodeId);
    void onSessionClosed();

private:
    void end(const QString &reason);
    void closeSession();

    std::unique_ptr<GnomeShellBackend> m_shell; // for the extension's availability and version
    std::unique_ptr<PipeWireStream> m_pipewire;
    Pick m_pick;
    QString m_session; // Mutter's screen cast session (object path)
    QString m_stream;
    bool m_busy = false;
    bool m_ended = false;
};
