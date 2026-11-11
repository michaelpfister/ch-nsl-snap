#include "capture/windows/WgcCapture.h"

#ifndef NOMINMAX
#define NOMINMAX // keep std::min and std::max usable
#endif
#include <windows.h>
// Classic COM support for C++/WinRT (com_ptr::as with Direct3D
// interfaces) needs <unknwn.h> before the first C++/WinRT header.
#include <unknwn.h>
#include <inspectable.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

// Interop between C++/WinRT and plain Win32/Direct3D: creating a capture
// item for a window handle, and reaching the Direct3D texture of a frame.
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>

namespace WgcCapture {

namespace {

using winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame;
using winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool;
using winrt::Windows::Graphics::Capture::GraphicsCaptureItem;
using winrt::Windows::Graphics::Capture::GraphicsCaptureSession;
using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
using winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
using winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface;

// How long the freeze waits for the windows' frames.
constexpr auto kFrameTimeout = std::chrono::milliseconds(1000);

Rect toRect(const RECT &r)
{
    return Rect{r.left, r.top, r.right, r.bottom};
}

bool isShellWindow(HWND hwnd)
{
    wchar_t name[64] = {};
    GetClassNameW(hwnd, name, 64);
    for (const wchar_t *shell : {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"}) {
        if (std::wcscmp(name, shell) == 0)
            return true;
    }
    return false;
}

bool isCapturable(HWND hwnd)
{
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || isShellWindow(hwnd))
        return false;
    // Cloaked: on another virtual desktop, or hidden by the system.
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked != 0)
        return false;
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW))
        return false; // palettes, overlays, our own picker
    // Menus, tooltips and helper windows have no title.
    return GetWindowTextLengthW(hwnd) > 0;
}

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM param)
{
    auto *list = reinterpret_cast<std::vector<Window> *>(param);
    if (!isCapturable(hwnd))
        return TRUE;
    Window window;
    window.hwnd = hwnd;
    RECT frame = {};
    RECT windowRect = {};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof frame))
        || !GetWindowRect(hwnd, &windowRect))
        return TRUE;
    window.frame = toRect(frame);
    window.windowRect = toRect(windowRect);
    if (window.frame.width() <= 0 || window.frame.height() <= 0)
        return TRUE;
    window.dpi = GetDpiForWindow(hwnd);
    window.maximized = IsZoomed(hwnd) != FALSE;
    // Not readable before Windows 11; then it stays at the default.
    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_DEFAULT;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners)))
        window.cornerPreference = int(corners);
    const int length = GetWindowTextLengthW(hwnd);
    window.title.resize(std::size_t(length) + 1);
    const int copied = GetWindowTextW(hwnd, window.title.data(), length + 1);
    window.title.resize(std::size_t(std::max(0, copied)));
    list->push_back(std::move(window));
    return TRUE; // continue with the next window
}

winrt::com_ptr<ID3D11Device> createDevice()
{
    // BGRA support is required for capture; WARP (software) if there is no GPU.
    winrt::com_ptr<ID3D11Device> device;
    for (const D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        if (SUCCEEDED(D3D11CreateDevice(nullptr, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                        D3D11_SDK_VERSION, device.put(), nullptr, nullptr)))
            return device;
        device = nullptr;
    }
    return nullptr;
}

IDirect3DDevice toWinrtDevice(const winrt::com_ptr<ID3D11Device> &device)
{
    const auto dxgi = device.as<IDXGIDevice>();
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put()));
    return inspectable.as<IDirect3DDevice>();
}

winrt::com_ptr<ID3D11Texture2D> textureOf(const IDirect3DSurface &surface)
{
    const auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
    return texture;
}

// One window's capture while the freeze waits for its first frame. The
// frame arrives on a thread-pool thread, hence the mutex and the event.
struct Pending
{
    Direct3D11CaptureFramePool pool{nullptr};
    GraphicsCaptureSession session{nullptr};
    HANDLE arrived = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::mutex mutex;
    Direct3D11CaptureFrame frame{nullptr};
    bool closed = false;

    ~Pending() { CloseHandle(arrived); }
};

} // namespace

bool isSupported()
{
    try {
        return GraphicsCaptureSession::IsSupported();
    } catch (...) {
        return false; // Windows before 1903: the class does not exist
    }
}

std::vector<Window> listWindows()
{
    // EnumWindows goes through the top-level windows from top to bottom.
    std::vector<Window> windows;
    EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

struct Freeze::Impl
{
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    std::vector<Window> windows;
    // Copies of the frames on the GPU; only the picked one is read back.
    std::vector<winrt::com_ptr<ID3D11Texture2D>> textures;
    std::vector<std::pair<int, int>> sizes;
};

Freeze::Freeze()
    : d(std::make_unique<Impl>())
{
}

Freeze::~Freeze() = default;

const std::vector<Window> &Freeze::windows() const
{
    return d->windows;
}

std::unique_ptr<Freeze> Freeze::capture()
{
    std::unique_ptr<Freeze> freeze(new Freeze);
    Impl &d = *freeze->d;
    try {
        d.device = createDevice();
        if (!d.device)
            return freeze;
        d.device->GetImmediateContext(d.context.put());
        const IDirect3DDevice device = toWinrtDevice(d.device);
        const auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();

        // Start all captures at once, so they show the same moment.
        const std::vector<Window> candidates = listWindows();
        std::vector<std::shared_ptr<Pending>> pending(candidates.size());
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            try {
                GraphicsCaptureItem item{nullptr};
                winrt::check_hresult(interop->CreateForWindow(
                    candidates[i].hwnd, winrt::guid_of<winrt::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                    winrt::put_abi(item)));
                const auto size = item.Size();
                if (size.Width <= 0 || size.Height <= 0)
                    continue;
                auto state = std::make_shared<Pending>();
                state->pool = Direct3D11CaptureFramePool::CreateFreeThreaded(
                    device, DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, size);
                state->session = state->pool.CreateCaptureSession(item);
                try {
                    state->session.IsCursorCaptureEnabled(false); // never the pointer (F-05)
                } catch (const winrt::hresult_error &) {
                }
                try {
                    state->session.IsBorderRequired(false); // no yellow border (Windows 11)
                } catch (const winrt::hresult_error &) {
                }
                std::weak_ptr<Pending> weak = state;
                state->pool.FrameArrived([weak](const auto &sender, const auto &) {
                    try {
                        const auto strong = weak.lock();
                        if (!strong)
                            return;
                        auto frame = sender.TryGetNextFrame();
                        std::lock_guard<std::mutex> lock(strong->mutex);
                        if (strong->closed || strong->frame || !frame)
                            return; // only the first frame counts
                        strong->frame = frame;
                        SetEvent(strong->arrived);
                    } catch (...) {
                        // The pool was closed meanwhile: nothing to keep.
                    }
                });
                state->session.StartCapture();
                pending[i] = state;
            } catch (const winrt::hresult_error &) {
                // Some windows cannot be captured (e.g. protected content).
            }
        }

        const auto deadline = std::chrono::steady_clock::now() + kFrameTimeout;
        for (const auto &state : pending) {
            if (!state)
                continue;
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            WaitForSingleObject(state->arrived, DWORD(std::max<long long>(0, left)));
        }

        // Keep a GPU copy of each frame, then end all captures.
        for (std::size_t i = 0; i < pending.size(); ++i) {
            const auto &state = pending[i];
            if (!state)
                continue;
            Direct3D11CaptureFrame frame{nullptr};
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                frame = state->frame;
                state->frame = nullptr;
                state->closed = true;
            }
            if (frame) {
                try {
                    const auto texture = textureOf(frame.Surface());
                    D3D11_TEXTURE2D_DESC desc = {};
                    texture->GetDesc(&desc);
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = 0;
                    desc.CPUAccessFlags = 0;
                    desc.MiscFlags = 0;
                    winrt::com_ptr<ID3D11Texture2D> copy;
                    winrt::check_hresult(d.device->CreateTexture2D(&desc, nullptr, copy.put()));
                    d.context->CopyResource(copy.get(), texture.get());
                    // The frame may be larger than the window's content.
                    const auto content = frame.ContentSize();
                    d.windows.push_back(candidates[i]);
                    d.textures.push_back(copy);
                    d.sizes.emplace_back(std::min(content.Width, int(desc.Width)),
                                         std::min(content.Height, int(desc.Height)));
                } catch (const winrt::hresult_error &) {
                }
                frame.Close();
            }
            state->session.Close();
            state->pool.Close();
        }
    } catch (const winrt::hresult_error &) {
        // No Direct3D or no capture support: an empty freeze.
    }
    return freeze;
}

bool Freeze::pixels(std::size_t index, Pixels &out) const
{
    if (index >= d->textures.size())
        return false;
    try {
        const auto &texture = d->textures[index];
        D3D11_TEXTURE2D_DESC desc = {};
        texture->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        winrt::com_ptr<ID3D11Texture2D> staging;
        winrt::check_hresult(d->device->CreateTexture2D(&desc, nullptr, staging.put()));
        d->context->CopyResource(staging.get(), texture.get());

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        winrt::check_hresult(d->context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
        const auto [width, height] = d->sizes[index];
        out.width = width;
        out.height = height;
        out.bgra.resize(std::size_t(width) * std::size_t(height) * 4);
        const auto *source = static_cast<const std::uint8_t *>(mapped.pData);
        for (int y = 0; y < height; ++y)
            std::memcpy(out.bgra.data() + std::size_t(y) * std::size_t(width) * 4,
                        source + std::size_t(y) * mapped.RowPitch, std::size_t(width) * 4);
        d->context->Unmap(staging.get(), 0);
        return true;
    } catch (const winrt::hresult_error &) {
        return false;
    }
}

bool windowRects(HWND hwnd, Rect &windowRect, Rect &frame)
{
    RECT r = {};
    RECT f = {};
    if (!IsWindow(hwnd) || !GetWindowRect(hwnd, &r)
        || FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &f, sizeof f)))
        return false;
    windowRect = toRect(r);
    frame = toRect(f);
    return true;
}

namespace {

// What the frame pool's callbacks share with the stream. They run on
// thread-pool threads: the mutex guards everything here, also the
// Direct3D context, which is not thread-safe. The callbacks hold only a
// weak pointer, so one that comes late finds nothing.
struct StreamState
{
    std::mutex mutex;
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    IDirect3DDevice winrtDevice{nullptr};
    winrt::Windows::Graphics::SizeInt32 poolSize{};
    // The newest frame, copied on the GPU right when it arrives, so the
    // pool gets its buffer back at once and may be recreated safely.
    winrt::com_ptr<ID3D11Texture2D> latest;
    int latestWidth = 0;  // the content inside it
    int latestHeight = 0;
    bool fresh = false;   // not read yet
    std::atomic<bool> closed{false};
    // For the tests' log: how many frames came, and how often the pool
    // got a new size.
    int arrived = 0;
    int recreated = 0;
    std::string lastError; // the last Direct3D or capture error, for the log

    void noteError(const char *where, const winrt::hresult_error &error)
    {
        char code[16] = {};
        std::snprintf(code, sizeof code, "0x%08X", unsigned(error.code().value));
        lastError = std::string(where) + ": " + winrt::to_string(error.message()) + " (" + code + ")";
    }

    // Keeps a GPU copy of `frame`; with the mutex held.
    void keep(const Direct3D11CaptureFrame &frame)
    {
        const auto texture = textureOf(frame.Surface());
        D3D11_TEXTURE2D_DESC desc = {};
        texture->GetDesc(&desc);
        D3D11_TEXTURE2D_DESC current = {};
        if (latest)
            latest->GetDesc(&current);
        if (!latest || current.Width != desc.Width || current.Height != desc.Height || current.Format != desc.Format) {
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            latest = nullptr;
            winrt::check_hresult(device->CreateTexture2D(&desc, nullptr, latest.put()));
        }
        context->CopyResource(latest.get(), texture.get());
        const auto content = frame.ContentSize();
        latestWidth = std::min(content.Width, int(desc.Width));
        latestHeight = std::min(content.Height, int(desc.Height));
        fresh = true;
    }
};

constexpr int kStreamBuffers = 2;

} // namespace

struct Stream::Impl
{
    GraphicsCaptureItem item{nullptr};
    Direct3D11CaptureFramePool pool{nullptr};
    GraphicsCaptureSession session{nullptr};
    winrt::event_token closedToken{};
    std::shared_ptr<StreamState> state = std::make_shared<StreamState>();
    winrt::com_ptr<ID3D11Texture2D> staging; // for reading back; reused while the size stays

    // `window`: the buffers are made as large as the whole desktop, so the
    // window still fits after it grew. Windows sends the next frame only
    // when the window changes again; a pool made for the new size after
    // the fact would leave a cut-off frame in the video until then.
    bool start(GraphicsCaptureItem captureItem, bool window)
    {
        item = captureItem;
        auto size = item.Size();
        if (size.Width <= 0 || size.Height <= 0)
            return false;
        if (window) {
            size.Width = std::max(size.Width, int32_t(GetSystemMetrics(SM_CXVIRTUALSCREEN)));
            size.Height = std::max(size.Height, int32_t(GetSystemMetrics(SM_CYVIRTUALSCREEN)));
        }
        state->poolSize = size;
        state->winrtDevice = toWinrtDevice(state->device);
        pool = Direct3D11CaptureFramePool::CreateFreeThreaded(
            state->winrtDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, kStreamBuffers, size);
        session = pool.CreateCaptureSession(item);
        try {
            session.IsCursorCaptureEnabled(true); // videos show the pointer (F-50)
        } catch (const winrt::hresult_error &) {
        }
        try {
            session.IsBorderRequired(false); // no yellow border (Windows 11)
        } catch (const winrt::hresult_error &) {
        }
        std::weak_ptr<StreamState> weak = state;
        pool.FrameArrived([weak](const Direct3D11CaptureFramePool &sender, const auto &) {
            try {
                const auto strong = weak.lock();
                if (!strong)
                    return;
                auto frame = sender.TryGetNextFrame();
                if (!frame)
                    return;
                std::lock_guard<std::mutex> lock(strong->mutex);
                if (strong->closed) {
                    frame.Close();
                    return;
                }
                ++strong->arrived;
                try {
                    strong->keep(frame);
                } catch (const winrt::hresult_error &error) {
                    strong->noteError("keep", error);
                }
                const auto content = frame.ContentSize();
                // Always given back: a frame kept open would take one of
                // the pool's two buffers for good.
                frame.Close();
                if (content.Width > strong->poolSize.Width || content.Height > strong->poolSize.Height) {
                    // Larger than the buffers (a window wider than the
                    // desktop, or a monitor whose resolution changed):
                    // larger buffers for the next frames. This one is kept,
                    // cut to the old size.
                    strong->poolSize.Width = std::max(content.Width, strong->poolSize.Width);
                    strong->poolSize.Height = std::max(content.Height, strong->poolSize.Height);
                    ++strong->recreated;
                    sender.Recreate(strong->winrtDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, kStreamBuffers,
                                    strong->poolSize);
                }
            } catch (...) {
                // The pool was closed meanwhile.
            }
        });
        std::weak_ptr<StreamState> weakClosed = state;
        closedToken = item.Closed([weakClosed](const auto &, const auto &) {
            if (const auto strong = weakClosed.lock())
                strong->closed = true;
        });
        session.StartCapture();
        return true;
    }

    ~Impl()
    {
        try {
            state->closed = true;
            if (item)
                item.Closed(closedToken);
            if (session)
                session.Close();
            if (pool)
                pool.Close();
        } catch (const winrt::hresult_error &) {
        }
    }
};

Stream::Stream()
    : d(std::make_unique<Impl>())
{
}

Stream::~Stream() = default;

namespace {

// Device and capture item for a stream; `create` makes the item.
using CreateItem = std::function<bool(IGraphicsCaptureItemInterop *interop, GraphicsCaptureItem &item)>;

std::unique_ptr<Stream> makeStream(std::unique_ptr<Stream> stream, Stream::Impl &d, bool window,
                                   const CreateItem &create)
{
    try {
        d.state->device = createDevice();
        if (!d.state->device)
            return nullptr;
        d.state->device->GetImmediateContext(d.state->context.put());
        const auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        GraphicsCaptureItem item{nullptr};
        if (!create(interop.get(), item) || !item || !d.start(item, window))
            return nullptr;
        return stream;
    } catch (const winrt::hresult_error &) {
        return nullptr;
    }
}

} // namespace

std::unique_ptr<Stream> Stream::forWindow(HWND hwnd)
{
    std::unique_ptr<Stream> stream(new Stream);
    Impl &d = *stream->d;
    return makeStream(std::move(stream), d, true, [hwnd](IGraphicsCaptureItemInterop *interop, GraphicsCaptureItem &item) {
        return SUCCEEDED(interop->CreateForWindow(
            hwnd, winrt::guid_of<winrt::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item)));
    });
}

std::unique_ptr<Stream> Stream::forMonitorAt(int x, int y)
{
    std::unique_ptr<Stream> stream(new Stream);
    Impl &d = *stream->d;
    HMONITOR monitor = MonitorFromPoint(POINT{x, y}, MONITOR_DEFAULTTONEAREST);
    return makeStream(std::move(stream), d, false, [monitor](IGraphicsCaptureItemInterop *interop, GraphicsCaptureItem &item) {
        return SUCCEEDED(interop->CreateForMonitor(
            monitor, winrt::guid_of<winrt::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item)));
    });
}

bool Stream::isClosed() const
{
    return d->state->closed;
}

std::string Stream::statistics() const
{
    std::lock_guard<std::mutex> lock(d->state->mutex);
    std::string text = "frames arrived " + std::to_string(d->state->arrived) + ", pool recreated "
                       + std::to_string(d->state->recreated);
    if (!d->state->lastError.empty())
        text += ", last error " + d->state->lastError;
    return text;
}

bool Stream::takeLatest(Pixels &out)
{
    StreamState &state = *d->state;
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.fresh || !state.latest)
        return false;
    state.fresh = false;
    try {
        D3D11_TEXTURE2D_DESC desc = {};
        state.latest->GetDesc(&desc);
        D3D11_TEXTURE2D_DESC stagingDesc = {};
        if (d->staging)
            d->staging->GetDesc(&stagingDesc);
        if (!d->staging || stagingDesc.Width != desc.Width || stagingDesc.Height != desc.Height
            || stagingDesc.Format != desc.Format) {
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.MiscFlags = 0;
            d->staging = nullptr;
            winrt::check_hresult(state.device->CreateTexture2D(&desc, nullptr, d->staging.put()));
        }
        state.context->CopyResource(d->staging.get(), state.latest.get());

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        winrt::check_hresult(state.context->Map(d->staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
        const int width = state.latestWidth;
        const int height = state.latestHeight;
        out.width = width;
        out.height = height;
        out.bgra.resize(std::size_t(width) * std::size_t(height) * 4);
        const auto *source = static_cast<const std::uint8_t *>(mapped.pData);
        for (int y = 0; y < height; ++y)
            std::memcpy(out.bgra.data() + std::size_t(y) * std::size_t(width) * 4,
                        source + std::size_t(y) * mapped.RowPitch, std::size_t(width) * 4);
        state.context->Unmap(d->staging.get(), 0);
        return width > 0 && height > 0;
    } catch (const winrt::hresult_error &error) {
        state.noteError("takeLatest", error);
        return false;
    }
}

} // namespace WgcCapture
