#include "capture/wayland/HyprlandIpc.h"

#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

#include <algorithm>

namespace HyprlandIpc {

namespace {

constexpr int kTimeoutMs = 1000;

QString socketPath()
{
    const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    // Since Hyprland 0.40 under XDG_RUNTIME_DIR, before in /tmp.
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    const QString current = runtime + QStringLiteral("/hypr/") + signature + QStringLiteral("/.socket.sock");
    if (!runtime.isEmpty() && QFileInfo::exists(current))
        return current;
    return QStringLiteral("/tmp/hypr/") + signature + QStringLiteral("/.socket.sock");
}

// "fullscreen" is a bool in older Hyprland versions, a mode (0-3) in newer ones.
bool isFullscreen(const QJsonValue &value)
{
    return value.isBool() ? value.toBool() : value.toInt() > 0;
}

QRect rectOf(const QJsonObject &client)
{
    const QJsonArray at = client.value(QLatin1String("at")).toArray();
    const QJsonArray size = client.value(QLatin1String("size")).toArray();
    if (at.size() < 2 || size.size() < 2)
        return {};
    return QRect(at[0].toInt(), at[1].toInt(), size[0].toInt(), size[1].toInt());
}

} // namespace

bool isRunning()
{
    return !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE");
}

QByteArray request(const QByteArray &command)
{
    if (!isRunning())
        return {};
    QLocalSocket socket;
    socket.connectToServer(socketPath());
    if (!socket.waitForConnected(kTimeoutMs))
        return {};
    socket.write(command);
    if (!socket.waitForBytesWritten(kTimeoutMs))
        return {};
    // Hyprland closes the connection after its answer.
    QByteArray answer;
    while (socket.state() == QLocalSocket::ConnectedState && socket.waitForReadyRead(kTimeoutMs))
        answer += socket.readAll();
    answer += socket.readAll();
    return answer;
}

QList<Window> visibleWindows(const QByteArray &clientsJson, const QByteArray &monitorsJson)
{
    struct Monitor
    {
        QString name;
        int workspace = 0;
        int special = 0; // id of the open special workspace (negative), 0 if none
    };
    QHash<int, Monitor> monitors;
    for (const QJsonValue &value : QJsonDocument::fromJson(monitorsJson).array()) {
        const QJsonObject monitor = value.toObject();
        monitors.insert(monitor.value(QLatin1String("id")).toInt(),
                        {monitor.value(QLatin1String("name")).toString(),
                         monitor.value(QLatin1String("activeWorkspace")).toObject().value(QLatin1String("id")).toInt(),
                         monitor.value(QLatin1String("specialWorkspace")).toObject().value(QLatin1String("id")).toInt()});
    }

    struct Candidate
    {
        Window window;
        int layer = 0;   // 0 = topmost group
        int recency = 0; // focusHistoryID: 0 = had the focus last
    };
    QList<Candidate> candidates;
    for (const QJsonValue &value : QJsonDocument::fromJson(clientsJson).array()) {
        const QJsonObject client = value.toObject();
        if (!client.value(QLatin1String("mapped")).toBool(true) || client.value(QLatin1String("hidden")).toBool())
            continue;
        const auto monitor = monitors.constFind(client.value(QLatin1String("monitor")).toInt(-1));
        if (monitor == monitors.cend())
            continue;
        const int workspace = client.value(QLatin1String("workspace")).toObject().value(QLatin1String("id")).toInt();
        const bool special = monitor->special != 0 && workspace == monitor->special;
        const bool pinned = client.value(QLatin1String("pinned")).toBool();
        if (!special && !pinned && workspace != monitor->workspace)
            continue;
        const QRect rect = rectOf(client);
        if (rect.isEmpty())
            continue;

        Candidate candidate;
        // "0x55d0c3a1b2c0": the window's address; the protocol takes its lower half.
        const QString address = client.value(QLatin1String("address")).toString();
        bool ok = false;
        const quint64 pointer = address.toULongLong(&ok, 16);
        if (!ok)
            continue;
        candidate.window.handle = uint32_t(pointer & 0xFFFFFFFFu);
        candidate.window.rect = rect;
        candidate.window.monitor = monitor->name;
        candidate.window.title = client.value(QLatin1String("title")).toString();
        candidate.window.appClass = client.value(QLatin1String("class")).toString();
        if (special)
            candidate.layer = 0;
        else if (pinned)
            candidate.layer = 1;
        else if (isFullscreen(client.value(QLatin1String("fullscreen"))))
            candidate.layer = 2;
        else if (client.value(QLatin1String("floating")).toBool())
            candidate.layer = 3;
        else
            candidate.layer = 4;
        candidate.recency = client.value(QLatin1String("focusHistoryID")).toInt(1 << 20);
        candidates.append(candidate);
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.layer != b.layer ? a.layer < b.layer : a.recency < b.recency;
    });
    QList<Window> windows;
    for (const Candidate &candidate : std::as_const(candidates))
        windows.append(candidate.window);
    return windows;
}

} // namespace HyprlandIpc
