#include "capture/macos/MacCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

// Objective-C++ (.mm), compiled with ARC (CMakeLists.txt): Objective-C
// objects are released automatically; Core Graphics objects (CGImageRef,
// CGContextRef) are not, they are retained and released by hand.
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <unistd.h>

#include <cmath>

#include <vector>

namespace {

// Windows smaller than this (in points) are helpers, not something to pick.
constexpr double kMinWindowSize = 20;

QRect toQRect(const CGRect &rect)
{
    return QRect(qRound(rect.origin.x), qRound(rect.origin.y), qRound(rect.size.width), qRound(rect.size.height));
}

QString toQString(NSString *string)
{
    return string ? QString::fromNSString(string) : QString();
}

// A Core Graphics image as a QImage with premultiplied alpha, its colours
// converted to sRGB (the colours of the web pages the captures end up in).
QImage toQImage(CGImageRef cgImage)
{
    if (!cgImage)
        return {};
    const int width = int(CGImageGetWidth(cgImage));
    const int height = int(CGImageGetHeight(cgImage));
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    CGColorSpaceRef sRgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    // BGRA in memory with premultiplied alpha first: Qt's
    // ARGB32_Premultiplied on little-endian machines (every Mac).
    const uint32_t layout = uint32_t(kCGImageAlphaPremultipliedFirst) | uint32_t(kCGBitmapByteOrder32Little);
    CGContextRef context = CGBitmapContextCreate(image.bits(), size_t(width), size_t(height), 8,
                                                 size_t(image.bytesPerLine()), sRgb, layout);
    CGColorSpaceRelease(sRgb);
    if (!context)
        return {};
    // Copy the pixels, alpha included, instead of blending them.
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
    CGContextRelease(context);
    return image;
}

// The size in pixels a filter's content has at its own resolution.
void sizeFor(SCContentFilter *filter, SCStreamConfiguration *config)
{
    const CGFloat scale = filter.pointPixelScale;
    config.width = size_t(std::lround(filter.contentRect.size.width * scale));
    config.height = size_t(std::lround(filter.contentRect.size.height * scale));
}

} // namespace

namespace MacCapture {

bool hasPermission()
{
    return CGPreflightScreenCaptureAccess();
}

void requestPermission()
{
    CGRequestScreenCaptureAccess();
}

struct Freeze::Impl
{
    QList<QImage> screens;
    QList<Window> windows;
    std::vector<CGImageRef> windowImages; // retained, one per window

    ~Impl()
    {
        for (CGImageRef image : windowImages)
            CGImageRelease(image);
    }
};

Freeze::Freeze(std::unique_ptr<Impl> impl)
    : m(std::move(impl))
{
}

Freeze::~Freeze() = default;

const QList<QImage> &Freeze::screens() const
{
    return m->screens;
}

const QList<Window> &Freeze::windows() const
{
    return m->windows;
}

QImage Freeze::windowImage(int index) const
{
    if (index < 0 || std::size_t(index) >= m->windowImages.size())
        return {};
    return toQImage(m->windowImages[std::size_t(index)]);
}

std::unique_ptr<Freeze> Freeze::capture(const QList<QScreen *> &screens, int timeoutMs, QString *diagnostic)
{
    @autoreleasepool {
        const dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW, int64_t(timeoutMs) * int64_t(NSEC_PER_MSEC));

        // What may be captured: the displays and the windows on screen.
        // ScreenCaptureKit answers on a queue of its own, so the main
        // thread can wait for it.
        __block SCShareableContent *content = nil;
        __block NSError *contentError = nil;
        dispatch_semaphore_t contentReady = dispatch_semaphore_create(0);
        [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                                   onScreenWindowsOnly:YES
                                                     completionHandler:^(SCShareableContent *found, NSError *error) {
                                                         content = found;
                                                         contentError = error;
                                                         dispatch_semaphore_signal(contentReady);
                                                     }];
        if (dispatch_semaphore_wait(contentReady, deadline) != 0) {
            *diagnostic = QStringLiteral("ScreenCaptureKit did not list the content in time");
            return nullptr;
        }
        if (!content) {
            *diagnostic = QStringLiteral("ScreenCaptureKit lists no content: %1")
                              .arg(toQString(contentError.localizedDescription));
            return nullptr;
        }

        // The display behind each of Qt's screens: the same rectangle in
        // points, top left of the main display at 0,0 for both.
        NSMutableArray<SCDisplay *> *displays = [NSMutableArray array];
        for (QScreen *screen : screens) {
            SCDisplay *match = nil;
            for (SCDisplay *display in content.displays) {
                if (toQRect(CGDisplayBounds(display.displayID)) == screen->geometry())
                    match = display;
            }
            if (!match) {
                *diagnostic = QStringLiteral("No display for the screen %1").arg(screen->name());
                return nullptr;
            }
            [displays addObject:match];
        }

        // The windows, topmost first: Core Graphics lists them in that
        // order, ScreenCaptureKit does not. Only normal windows (layer 0)
        // of other apps, visible and not tiny.
        NSMutableDictionary<NSNumber *, SCWindow *> *byId = [NSMutableDictionary dictionary];
        for (SCWindow *window in content.windows)
            byId[@(window.windowID)] = window;
        NSArray *order = CFBridgingRelease(CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));
        const pid_t ownPid = getpid();
        NSMutableArray<SCWindow *> *windows = [NSMutableArray array];
        QList<Window> windowInfo;
        for (NSDictionary *entry in order) {
            NSNumber *number = entry[(__bridge NSString *)kCGWindowNumber];
            SCWindow *window = number ? byId[number] : nil;
            if (!window || !window.isOnScreen || window.windowLayer != 0)
                continue;
            if (window.owningApplication.processID == ownPid)
                continue;
            if ([entry[(__bridge NSString *)kCGWindowAlpha] doubleValue] <= 0)
                continue;
            CGRect bounds = CGRectZero;
            NSDictionary *boundsEntry = entry[(__bridge NSString *)kCGWindowBounds];
            if (!boundsEntry || !CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)boundsEntry, &bounds))
                continue;
            if (bounds.size.width < kMinWindowSize || bounds.size.height < kMinWindowSize)
                continue;
            [windows addObject:window];
            windowInfo.append({toQRect(bounds), toQString(window.title),
                               toQString(window.owningApplication.applicationName),
                               toQString(window.owningApplication.bundleIdentifier)});
        }

        // Everything at once (D-011): one picture per display and per
        // window, each window on its own, without its shadow and with
        // transparent corners. The answers arrive on other threads; the
        // dictionary outlives this function if one comes too late.
        NSMutableDictionary<NSNumber *, id> *pictures = [NSMutableDictionary dictionary];
        dispatch_group_t group = dispatch_group_create();
        auto request = ^(SCContentFilter *filter, SCStreamConfiguration *config, NSInteger key) {
            dispatch_group_enter(group);
            [SCScreenshotManager captureImageWithFilter:filter
                                          configuration:config
                                      completionHandler:^(CGImageRef image, NSError *error) {
                                          if (image && !error) {
                                              @synchronized(pictures) {
                                                  pictures[@(key)] = (__bridge id)image;
                                              }
                                          }
                                          dispatch_group_leave(group);
                                      }];
        };
        for (NSUInteger i = 0; i < displays.count; ++i) {
            SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:displays[i] excludingWindows:@[]];
            SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
            sizeFor(filter, config);
            config.showsCursor = NO;
            request(filter, config, -NSInteger(i) - 1); // displays: -1, -2, ...
        }
        for (NSUInteger i = 0; i < windows.count; ++i) {
            SCContentFilter *filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:windows[i]];
            SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
            sizeFor(filter, config);
            config.showsCursor = NO;
            config.ignoreShadowsSingleWindow = YES; // no shadow (D-004)
            config.shouldBeOpaque = NO;             // transparent corners
            request(filter, config, NSInteger(i));
        }
        if (dispatch_group_wait(group, deadline) != 0) {
            *diagnostic = QStringLiteral("ScreenCaptureKit did not deliver every picture in time");
            return nullptr;
        }

        auto impl = std::make_unique<Impl>();
        @synchronized(pictures) {
            for (NSUInteger i = 0; i < displays.count; ++i) {
                const QImage picture = toQImage((__bridge CGImageRef)pictures[@(-NSInteger(i) - 1)]);
                if (picture.isNull()) {
                    *diagnostic = QStringLiteral("No picture of the screen %1").arg(screens[qsizetype(i)]->name());
                    return nullptr;
                }
                // Screens are opaque; RGB32 keeps the overlay fast.
                impl->screens.append(picture.convertToFormat(QImage::Format_RGB32));
            }
            for (NSUInteger i = 0; i < windows.count; ++i) {
                CGImageRef picture = (__bridge CGImageRef)pictures[@(NSInteger(i))];
                if (!picture)
                    continue; // e.g. closed in the meantime
                impl->windows.append(windowInfo[qsizetype(i)]);
                impl->windowImages.push_back(CGImageRetain(picture));
            }
        }
        return std::unique_ptr<Freeze>(new Freeze(std::move(impl)));
    }
}

void raiseAboveAll(QWidget *widget)
{
    // On macOS a widget's window id is its NSView, but only with Qt's
    // Cocoa platform (the tests draw offscreen, where it is something else).
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        return;
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
    NSWindow *window = view.window;
    if (!window)
        return;
    // Above the menu bar and the Dock, on every Space, also next to an app
    // in full screen; Qt's "stay on top" stops below the menu bar.
    window.level = NSScreenSaverWindowLevel;
    window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                                | NSWindowCollectionBehaviorFullScreenAuxiliary
                                | NSWindowCollectionBehaviorStationary;
    // Qt makes tool windows panels, which macOS hides when the app is not
    // active.
    window.hidesOnDeactivate = NO;
    [NSApp activate];
    [window makeKeyAndOrderFront:nil];
}

} // namespace MacCapture
