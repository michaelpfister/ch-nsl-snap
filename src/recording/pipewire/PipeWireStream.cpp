#include "recording/pipewire/PipeWireStream.h"

#include <pipewire/pipewire.h>
#include <spa/buffer/meta.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <atomic>
#include <mutex>

namespace {

// pw_init() once per process; PipeWire keeps it until the process ends.
void initPipeWire()
{
    static std::once_flag once;
    std::call_once(once, [] { pw_init(nullptr, nullptr); });
}

QImage::Format imageFormat(uint32_t spaFormat)
{
    // SPA names list the bytes in memory order; Qt's 32-bit formats are
    // words on a little-endian machine: B, G, R, X in memory is Qt's RGB32.
    switch (spaFormat) {
    case SPA_VIDEO_FORMAT_BGRx: return QImage::Format_RGB32;
    case SPA_VIDEO_FORMAT_BGRA: return QImage::Format_ARGB32_Premultiplied;
    case SPA_VIDEO_FORMAT_RGBx: return QImage::Format_RGBX8888;
    case SPA_VIDEO_FORMAT_RGBA: return QImage::Format_RGBA8888_Premultiplied;
    default: return QImage::Format_Invalid;
    }
}

} // namespace

struct PipeWireStream::Impl
{
    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_stream *stream = nullptr;
    spa_hook coreListener = {};
    spa_hook streamListener = {};

    // Set in param_changed, read in process: both on PipeWire's thread.
    spa_video_info_raw format = {};
    bool haveFormat = false;

    mutable std::mutex mutex;
    QImage latest;
    bool fresh = false;
    QString error;
    std::atomic<bool> ended{false};

    void fail(const QString &message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (error.isEmpty())
            error = message;
        ended = true;
    }

    // The format is agreed: ask for buffers in shared memory, with the
    // header (to skip broken frames) and the crop region.
    void onFormat(const spa_pod *param)
    {
        uint32_t mediaType = 0;
        uint32_t mediaSubtype = 0;
        if (spa_format_parse(param, &mediaType, &mediaSubtype) < 0 || mediaType != SPA_MEDIA_TYPE_video
            || mediaSubtype != SPA_MEDIA_SUBTYPE_raw)
            return;
        if (spa_format_video_raw_parse(param, &format) < 0)
            return;
        haveFormat = true;

        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const spa_pod *params[3];
        params[0] = static_cast<const spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 16),
            SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr))));
        params[1] = static_cast<const spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
            SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
            SPA_PARAM_META_size, SPA_POD_Int(sizeof(spa_meta_header))));
        params[2] = static_cast<const spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
            SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoCrop),
            SPA_PARAM_META_size, SPA_POD_Int(sizeof(spa_meta_region))));
        pw_stream_update_params(stream, params, 3);
    }

    // A new buffer: copy it and give it back at once.
    void onProcess()
    {
        pw_buffer *buffer = pw_stream_dequeue_buffer(stream);
        if (!buffer)
            return;
        const spa_buffer *spa = buffer->buffer;
        const auto *header =
            static_cast<const spa_meta_header *>(spa_buffer_find_meta_data(spa, SPA_META_Header, sizeof(spa_meta_header)));
        const spa_data &data = spa->datas[0];
        // Size 0: only the pointer moved, the picture is the same.
        const bool usable = haveFormat && data.data && data.chunk && data.chunk->size > 0
                            && !(header && (header->flags & SPA_META_HEADER_FLAG_CORRUPTED));
        if (usable) {
            QRect crop;
            const auto *region =
                static_cast<const spa_meta_region *>(spa_buffer_find_meta_data(spa, SPA_META_VideoCrop, sizeof(spa_meta_region)));
            if (region && spa_meta_region_is_valid(region))
                crop = QRect(region->region.position.x, region->region.position.y, int(region->region.size.width),
                             int(region->region.size.height));
            const auto *bytes = static_cast<const uchar *>(data.data) + data.chunk->offset;
            const int stride = data.chunk->stride > 0 ? data.chunk->stride : int(format.size.width) * 4;
            QImage image = toImage(bytes, int(format.size.width), int(format.size.height), stride, format.format, crop);
            if (!image.isNull()) {
                std::lock_guard<std::mutex> lock(mutex);
                latest = std::move(image);
                fresh = true;
            }
        }
        pw_stream_queue_buffer(stream, buffer);
    }
};

namespace {

// The C callbacks, forwarding to the Impl in `data`.
PipeWireStream::Impl *implOf(void *data)
{
    return static_cast<PipeWireStream::Impl *>(data);
}

void onStateChanged(void *data, pw_stream_state, pw_stream_state state, const char *message)
{
    if (state == PW_STREAM_STATE_ERROR)
        implOf(data)->fail(QString::fromUtf8(message ? message : "PipeWire error"));
    else if (state == PW_STREAM_STATE_UNCONNECTED)
        implOf(data)->fail(QStringLiteral("The stream was closed."));
}

void onParamChanged(void *data, uint32_t id, const spa_pod *param)
{
    if (param && id == SPA_PARAM_Format)
        implOf(data)->onFormat(param);
}

void onProcess(void *data)
{
    implOf(data)->onProcess();
}

void onCoreError(void *data, uint32_t, int, int, const char *message)
{
    implOf(data)->fail(QString::fromUtf8(message ? message : "PipeWire error"));
}

// The event tables: zero (no callback) except the events the stream uses.
pw_stream_events makeStreamEvents()
{
    pw_stream_events events = {};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.state_changed = onStateChanged;
    events.param_changed = onParamChanged;
    events.process = onProcess;
    return events;
}

pw_core_events makeCoreEvents()
{
    pw_core_events events = {};
    events.version = PW_VERSION_CORE_EVENTS;
    events.error = onCoreError;
    return events;
}

const pw_stream_events kStreamEvents = makeStreamEvents();
const pw_core_events kCoreEvents = makeCoreEvents();

} // namespace

PipeWireStream::PipeWireStream()
    : d(std::make_unique<Impl>())
{
}

PipeWireStream::~PipeWireStream()
{
    stop();
}

bool PipeWireStream::start(uint32_t nodeId, int fd)
{
    stop();
    initPipeWire();
    d->ended = false;
    d->haveFormat = false;
    d->error.clear();
    d->fresh = false;

    d->loop = pw_thread_loop_new("snapslash-video", nullptr);
    if (!d->loop)
        return false;
    d->context = pw_context_new(pw_thread_loop_get_loop(d->loop), nullptr, 0);
    if (!d->context || pw_thread_loop_start(d->loop) < 0) {
        stop();
        return false;
    }

    pw_thread_loop_lock(d->loop);
    d->core = fd >= 0 ? pw_context_connect_fd(d->context, fd, nullptr, 0) : pw_context_connect(d->context, nullptr, 0);
    if (!d->core) {
        pw_thread_loop_unlock(d->loop);
        stop();
        return false;
    }
    pw_core_add_listener(d->core, &d->coreListener, &kCoreEvents, d.get());
    d->stream = pw_stream_new(d->core, "Snapslash",
                              pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
                                                PW_KEY_MEDIA_ROLE, "Screen", nullptr));
    if (!d->stream) {
        pw_thread_loop_unlock(d->loop);
        stop();
        return false;
    }
    pw_stream_add_listener(d->stream, &d->streamListener, &kStreamEvents, d.get());

    // What the app takes: raw video in one of four 32-bit layouts, any size
    // and rate. Without a "modifier" the other side keeps to shared memory.
    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_rectangle defaultSize = SPA_RECTANGLE(1920, 1080);
    spa_rectangle minSize = SPA_RECTANGLE(1, 1);
    spa_rectangle maxSize = SPA_RECTANGLE(16384, 16384);
    spa_fraction defaultRate = SPA_FRACTION(30, 1);
    spa_fraction minRate = SPA_FRACTION(0, 1);
    spa_fraction maxRate = SPA_FRACTION(1000, 1);
    const spa_pod *params[1];
    params[0] = static_cast<const spa_pod *>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx,
                                                        SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
        SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&defaultSize, &minSize, &maxSize),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&defaultRate, &minRate, &maxRate)));
    const int result = pw_stream_connect(d->stream, PW_DIRECTION_INPUT, nodeId,
                                         pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                                         params, 1);
    pw_thread_loop_unlock(d->loop);
    if (result < 0) {
        stop();
        return false;
    }
    return true;
}

void PipeWireStream::stop()
{
    if (d->loop)
        pw_thread_loop_lock(d->loop);
    if (d->stream) {
        spa_hook_remove(&d->streamListener);
        pw_stream_disconnect(d->stream);
        pw_stream_destroy(d->stream);
        d->stream = nullptr;
    }
    if (d->core) {
        spa_hook_remove(&d->coreListener);
        pw_core_disconnect(d->core);
        d->core = nullptr;
    }
    if (d->loop) {
        pw_thread_loop_unlock(d->loop);
        pw_thread_loop_stop(d->loop);
    }
    if (d->context) {
        pw_context_destroy(d->context);
        d->context = nullptr;
    }
    if (d->loop) {
        pw_thread_loop_destroy(d->loop);
        d->loop = nullptr;
    }
}

bool PipeWireStream::takeLatest(QImage &out)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    if (!d->fresh)
        return false;
    d->fresh = false;
    out = d->latest;
    return true;
}

bool PipeWireStream::hasEnded() const
{
    return d->ended;
}

QString PipeWireStream::lastError() const
{
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->error;
}

QImage PipeWireStream::toImage(const uchar *data, int width, int height, int stride, uint32_t spaFormat,
                               const QRect &crop)
{
    const QImage::Format qtFormat = imageFormat(spaFormat);
    if (!data || width <= 0 || height <= 0 || qtFormat == QImage::Format_Invalid)
        return {};
    // Wraps PipeWire's memory without copying; copy() makes the image its own.
    const QImage wrapped(data, width, height, stride, qtFormat);
    const QRect part = crop.isValid() ? crop.intersected(wrapped.rect()) : wrapped.rect();
    if (part.isEmpty())
        return {};
    QImage image = wrapped.copy(part);
    // Qt expects the unused fourth byte of BGRx and RGBx (its RGB32 and
    // RGBX8888) to be 0xff; PipeWire often leaves it at 0.
    if (spaFormat == SPA_VIDEO_FORMAT_BGRx || spaFormat == SPA_VIDEO_FORMAT_RGBx) {
        for (int y = 0; y < image.height(); ++y) {
            uchar *line = image.scanLine(y);
            for (int x = 0; x < image.width(); ++x)
                line[x * 4 + 3] = 0xff;
        }
    }
    return image;
}
