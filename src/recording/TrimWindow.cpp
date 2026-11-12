#include "recording/TrimWindow.h"

#include "recording/RecordingIndicator.h"
#include "recording/TrimTimeline.h"
#include "recording/VideoTrimmer.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>

// The frame the player shows, scaled into the widget with its aspect ratio.
class VideoPreview : public QWidget
{
public:
    explicit VideoPreview(QWidget *parent)
        : QWidget(parent)
    {
        setMinimumSize(320, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setFrame(const QImage &frame)
    {
        m_frame = frame;
        update();
    }
    const QImage &frame() const { return m_frame; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Theme::current().canvas);
        if (m_frame.isNull())
            return;
        const QSize size = m_frame.size().scaled(this->size(), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(target, m_frame);
    }

private:
    QImage m_frame;
};

namespace {

// "0:12.4": the time with tenths, so single frames show a difference.
QString timeWithTenths(qint64 ms)
{
    return RecordingIndicator::formatTime(ms) + QStringLiteral(".%1").arg(ms % 1000 / 100);
}

} // namespace

TrimWindow::TrimWindow(const QString &recording, const QString &title, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_recording(recording)
{
    setObjectName(QStringLiteral("TrimWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setWindowTitle(title.isEmpty() ? tr("Video zuschneiden") : tr("Video zuschneiden – %1").arg(title));
    resize(960, 680);

    m_preview = new VideoPreview(this);
    m_timeline = new TrimTimeline(this);
    m_playButton = new QToolButton(this);
    m_playButton->setProperty("role", QStringLiteral("icon"));
    m_playButton->setIconSize(QSize(20, 20));
    m_playButton->setToolTip(tr("Abspielen / Anhalten (Leertaste)"));
    m_playButton->setAccessibleName(tr("Abspielen / Anhalten"));
    m_playButton->setFocusPolicy(Qt::NoFocus);
    m_time = new QLabel(this);
    m_range = new QLabel(this);
    m_range->setProperty("role", QStringLiteral("muted"));
    auto *hint = new QLabel(tr("Griffe ziehen oder Pfeiltasten: 1 Bild, mit Shift 1 s · Pfeil hoch/runter: "
                               "anderer Griff · Leertaste: Abspielen"),
                            this);
    hint->setProperty("role", QStringLiteral("hint"));
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->setVisible(false);

    m_discardButton = new QPushButton(tr("Verwerfen"), this);
    m_discardButton->setProperty("role", QStringLiteral("secondary"));
    m_discardButton->setToolTip(tr("Die Aufnahme löschen"));
    m_wholeButton = new QPushButton(tr("Ganz speichern"), this);
    m_wholeButton->setProperty("role", QStringLiteral("secondary"));
    m_wholeButton->setToolTip(tr("Die ganze Aufnahme speichern, ohne zu schneiden"));
    m_saveButton = new QPushButton(tr("Speichern"), this);
    m_saveButton->setProperty("role", QStringLiteral("primary"));
    m_saveButton->setToolTip(tr("Nur den Teil zwischen den Griffen speichern"));
    m_saveButton->setDefault(true);
    m_discardButton->setObjectName(QStringLiteral("discard"));
    m_wholeButton->setObjectName(QStringLiteral("saveWhole"));
    m_saveButton->setObjectName(QStringLiteral("save"));
    for (QPushButton *button : {m_discardButton, m_wholeButton, m_saveButton})
        button->setFocusPolicy(Qt::NoFocus); // the keys belong to the timeline

    const Theme theme = Theme::current();
    m_playButton->setIcon(Icons::icon(QStringLiteral("play"), theme.text));
    m_saveButton->setIcon(Icons::icon(QStringLiteral("save"), QColor(Qt::white)));

    auto *controls = new QHBoxLayout;
    controls->addWidget(m_playButton);
    controls->addWidget(m_time);
    controls->addStretch(1);
    controls->addWidget(m_range);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_discardButton);
    buttons->addStretch(1);
    buttons->addWidget(m_wholeButton);
    buttons->addWidget(m_saveButton);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);
    layout->addWidget(m_preview, 1);
    layout->addLayout(controls);
    layout->addWidget(m_timeline);
    layout->addWidget(hint);
    layout->addWidget(m_progress);
    layout->addLayout(buttons);

    connect(m_playButton, &QToolButton::clicked, this, &TrimWindow::togglePlay);
    connect(m_discardButton, &QPushButton::clicked, this, &TrimWindow::discard);
    connect(m_wholeButton, &QPushButton::clicked, this, &TrimWindow::saveWhole);
    connect(m_saveButton, &QPushButton::clicked, this, &TrimWindow::saveRange);
    connect(m_timeline, &TrimTimeline::rangeChanged, this, &TrimWindow::updateLabels);
    connect(m_timeline, &TrimTimeline::handleMoved, this, &TrimWindow::showFrame);
    connect(m_timeline, &TrimTimeline::positionRequested, this, &TrimWindow::showFrame);

    // The preview: the player paints into a sink, the sink into the widget.
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        if (frame.isValid())
            m_preview->setFrame(frame.toImage());
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia && !m_loaded)
            onLoaded();
        else if (status == QMediaPlayer::EndOfMedia)
            m_playButton->setIcon(Icons::icon(QStringLiteral("play"), Theme::current().text));
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, &TrimWindow::onPositionChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        const QString icon = state == QMediaPlayer::PlayingState ? QStringLiteral("pause") : QStringLiteral("play");
        m_playButton->setIcon(Icons::icon(icon, Theme::current().text));
    });
    setBusy(true); // until the recording is loaded
    m_progress->setVisible(false);
    m_player->setSource(QUrl::fromLocalFile(recording));
    m_timeline->setFocus();
}

TrimWindow::~TrimWindow()
{
    // The player holds the file open; it must let go before the delete.
    m_player->stop();
    m_player->setSource(QUrl());
    if (m_trimmer)
        m_trimmer->cancel();
    // Whatever was not handed over goes.
    QFile::remove(m_recording);
    if (!m_trimmed.isEmpty())
        QFile::remove(m_trimmed);
}

bool TrimWindow::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

QImage TrimWindow::currentFrame() const
{
    return m_preview->frame();
}

void TrimWindow::onLoaded()
{
    m_loaded = true;
    m_timeline->setFrameCount(TrimTimeline::msToFrame(m_player->duration()));
    setBusy(false);
    // The first frame, without playing.
    m_player->pause();
    showFrame(0);
    updateLabels();
}

void TrimWindow::onPositionChanged(qint64 ms)
{
    const int frame = TrimTimeline::msToFrame(ms);
    m_timeline->setPosition(frame);
    // Playing stops at the end handle.
    if (isPlaying() && frame >= m_timeline->endFrame()) {
        m_player->pause();
        m_player->setPosition(TrimTimeline::frameToMs(m_timeline->endFrame() - 1));
    }
    updateLabels();
}

void TrimWindow::showFrame(int frame)
{
    if (!m_loaded)
        return;
    m_player->pause();
    m_player->setPosition(TrimTimeline::frameToMs(frame));
    m_timeline->setPosition(frame);
    updateLabels();
}

void TrimWindow::togglePlay()
{
    if (!m_loaded || (m_trimmer && m_trimmer->isRunning()))
        return;
    if (isPlaying()) {
        m_player->pause();
        return;
    }
    // From the start handle if the position is outside the part.
    const int frame = TrimTimeline::msToFrame(m_player->position());
    if (frame < m_timeline->startFrame() || frame >= m_timeline->endFrame() - 1)
        m_player->setPosition(TrimTimeline::frameToMs(m_timeline->startFrame()));
    m_player->play();
}

void TrimWindow::updateLabels()
{
    const qint64 total = m_loaded ? m_player->duration() : 0;
    m_time->setText(QStringLiteral("%1 / %2").arg(timeWithTenths(m_player->position()), timeWithTenths(total)));
    const qint64 start = TrimTimeline::frameToMs(m_timeline->startFrame());
    const qint64 end = TrimTimeline::frameToMs(m_timeline->endFrame());
    m_range->setText(tr("Behalten: %1 bis %2 (%3)")
                         .arg(timeWithTenths(start), timeWithTenths(end), timeWithTenths(end - start)));
    m_saveButton->setText(m_timeline->isWhole() ? tr("Speichern") : tr("Ausschnitt speichern"));
}

void TrimWindow::setBusy(bool busy)
{
    for (QWidget *widget : std::initializer_list<QWidget *>{m_discardButton, m_wholeButton, m_saveButton,
                                                            m_playButton, m_timeline})
        widget->setEnabled(!busy);
    m_progress->setVisible(busy);
}

void TrimWindow::saveWhole()
{
    if (!m_loaded || m_done)
        return;
    finishWith(m_recording);
}

void TrimWindow::saveRange()
{
    if (!m_loaded || m_done || (m_trimmer && m_trimmer->isRunning()))
        return;
    if (m_timeline->isWhole()) {
        finishWith(m_recording);
        return;
    }
    m_player->pause();
    // Next to the recording in the temporary folder.
    const QFileInfo info(m_recording);
    m_trimmed = info.dir().filePath(info.completeBaseName() + QStringLiteral("-trimmed.mp4"));
    QFile::remove(m_trimmed);

    m_trimmer = new VideoTrimmer(this);
    m_trimmer->setVideoCodec(m_codec);
    connect(m_trimmer, &VideoTrimmer::progress, m_progress, &QProgressBar::setValue);
    connect(m_trimmer, &VideoTrimmer::finished, this, [this] {
        m_trimmer->deleteLater();
        m_trimmer = nullptr;
        setBusy(false);
        finishWith(m_trimmed);
    });
    connect(m_trimmer, &VideoTrimmer::failed, this, [this](const QString &message) {
        m_trimmer->deleteLater();
        m_trimmer = nullptr;
        setBusy(false);
        QMessageBox::warning(this, tr("Video zuschneiden"), tr("Der Ausschnitt konnte nicht gespeichert werden: %1")
                                                                .arg(message));
    });
    setBusy(true);
    m_progress->setValue(0);
    m_trimmer->start(m_recording, m_trimmed, TrimTimeline::frameToMs(m_timeline->startFrame()),
                     TrimTimeline::frameToMs(m_timeline->endFrame()));
}

void TrimWindow::finishWith(QString path)
{
    m_done = true;
    // Handed over: not deleted when the window goes.
    if (path == m_recording)
        m_recording.clear();
    else if (path == m_trimmed)
        m_trimmed.clear();
    m_player->stop();
    m_player->setSource(QUrl()); // lets go of the file before it is moved
    emit saved(path);
    close();
}

void TrimWindow::discard()
{
    if (m_done)
        return;
    m_done = true;
    emit discarded();
    close();
}

void TrimWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        togglePlay();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        saveRange();
        return;
    case Qt::Key_Escape:
        close();
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

void TrimWindow::closeEvent(QCloseEvent *event)
{
    if (m_done) {
        event->accept();
        return;
    }
    // Writing the part: wait for it.
    if (m_trimmer && m_trimmer->isRunning()) {
        event->ignore();
        return;
    }
    // Closing without a choice would lose the recording: ask.
    QMessageBox box(QMessageBox::Question, tr("Video verwerfen"), tr("Video verwerfen?"), QMessageBox::NoButton, this);
    box.setInformativeText(tr("Die Aufnahme ist nicht gespeichert."));
    QPushButton *discard = box.addButton(tr("Verwerfen"), QMessageBox::DestructiveRole);
    box.setDefaultButton(box.addButton(QMessageBox::Cancel));
    box.exec();
    if (box.clickedButton() != discard) {
        event->ignore();
        return;
    }
    m_done = true;
    emit discarded();
    event->accept();
}
