#include "keyboard-matcher.h"
#include "pen-buttons.h"
#include "soft-area.h"
#include "shortcut-binding.h"
#include "action-state.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>
#include <QTextStream>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <map>
#include <vector>

// Only the two interfaces of the supported 1060PRO are opened. The grabs are
// released by the kernel if this process exits for any reason.
static constexpr auto penPath =
    "/dev/input/by-id/usb-GAOMON_GAOMON_Tablet_1060PRO-if01-event-mouse";
static constexpr auto keyPath = "/dev/input/by-id/usb-GAOMON_GAOMON_Tablet_1060PRO-if02-event-kbd";
class Helper : public QObject {
    int penFd = -1, keyFd = -1;
    libevdev *pen = nullptr, *keyboard = nullptr, *outputKeys = nullptr, *outputMouse = nullptr;
    libevdev_uinput *virtualPen = nullptr, *virtualKeys = nullptr, *virtualMouse = nullptr;
    QSocketNotifier *penNotify = nullptr, *keyNotify = nullptr, *commands = nullptr;
    QByteArray buffer;
    std::map<std::pair<int, int>, Binding> hard;
    std::map<int, Binding> soft, penBindings;
    ActionState actionState;
    PenButtons penButtons;
    std::vector<input_event> keyboardFrame;
    KeyboardMatcher keyMatcher;
    std::map<int, Binding> activeHard;
    Binding activeSoft;
    bool softDown = false, inSoft = false, touch = false, proximity = false, softEnabled = false;
    QJsonObject pendingConfig;
    bool hasPendingConfig = false;
    int x = 0, y = 0, xmin = 0, xmax = 1, ymin = 0, ymax = 1;
    double softHeight = softAreaHeight;
    std::vector<input_event> frame;
    void send(libevdev_uinput *dev, int type, int code, int val) {
        if (libevdev_uinput_write_event(dev, type, code, val) < 0) {
            fprintf(stderr, "uinput write failed\n");
            QCoreApplication::exit(1);
        }
    }
    void sync(libevdev_uinput *dev) {
        send(dev, EV_SYN, SYN_REPORT, 0);
    }
    void action(const Binding &b, bool down) {
        actionState.apply(
            b, down,
            [this](bool mouse, int code, int value) {
                send(mouse ? virtualMouse : virtualKeys, EV_KEY, code, value);
            },
            [this](bool mouse) { sync(mouse ? virtualMouse : virtualKeys); });
    }
    void applyPending() {
        if (!hasPendingConfig || keyMatcher.busy() || penButtons.busy() || softDown || touch)
            return;
        const auto root = pendingConfig;
        std::map<std::pair<int, int>, Binding> h;
        std::map<int, Binding> s, p;
        for (auto v : root.value("hard").toArray()) {
            auto o = v.toObject();
            int code = o.value("source").toInt(), mods = o.value("sourceMods").toInt();
            auto b = parseBinding(o);
            if (code > 0 && code <= KEY_MAX && mods >= 0 && mods <= 15 && b)
                h[{code, mods}] = *b;
        }
        for (auto v : root.value("soft").toArray()) {
            auto o = v.toObject();
            int index = o.value("source").toInt(-1);
            auto b = parseBinding(o);
            if (index >= 0 && index < 16 && b)
                s[index] = *b;
        }
        for (auto v : root.value("pen").toArray()) {
            auto o = v.toObject();
            int code = o.value("source").toInt();
            auto b = parseBinding(o);
            if ((code == BTN_STYLUS || code == BTN_STYLUS2) && b)
                p[code] = *b;
        }
        hard = std::move(h);
        soft = std::move(s);
        penBindings = std::move(p);
        softHeight = qBound(0.005, root.value("height").toDouble(softAreaHeight), 0.20);
        softEnabled = root.value("softEnabled").toBool(false);
        hasPendingConfig = false;
        puts("configured");
        fflush(stdout);
    }
    void readCommands() {
        char data[4096];
        auto n = ::read(STDIN_FILENO, data, sizeof(data));
        if (n <= 0) {
            QCoreApplication::quit();
            return;
        }
        buffer.append(data, n);
        while (buffer.contains('\n')) {
            auto at = buffer.indexOf('\n');
            auto line = buffer.left(at);
            buffer.remove(0, at + 1);
            QJsonParseError error;
            auto doc = QJsonDocument::fromJson(line, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject())
                continue;
            pendingConfig = doc.object();
            hasPendingConfig = true;
            applyPending();
        }
    }
    void keyboardEvents() {
        input_event e;
        int rc;
        while ((rc = libevdev_next_event(keyboard, LIBEVDEV_READ_FLAG_NORMAL, &e)) == 0) {
            keyboardFrame.push_back(e);
            if (e.type != EV_SYN || e.code != SYN_REPORT)
                continue;
            auto lookup = [this](int code, int mods) -> const Binding * {
                // This 1060PRO emits B/E from its dedicated keyboard interface
                // at the same time as the two physical pen buttons.
                if (mods == 0 && (code == KEY_B || code == KEY_E)) {
                    const int penCode = code == KEY_B ? BTN_STYLUS : BTN_STYLUS2;
                    auto p = penBindings.find(penCode);
                    if (p != penBindings.end())
                        return &p->second;
                }
                auto h = hard.find({code, mods});
                return h == hard.end() ? nullptr : &h->second;
            };
            auto decision = keyMatcher.process(
                keyboardFrame, [&](int code, int mods) { return lookup(code, mods) != nullptr; });
            for (int code : decision.presses) {
                activeHard[code] = *lookup(code, keyMatcher.currentModifiers());
                action(activeHard[code], true);
            }
            for (int code : decision.releases) {
                auto it = activeHard.find(code);
                if (it != activeHard.end()) {
                    action(it->second, false);
                    activeHard.erase(it);
                }
            }
            if (!decision.consume)
                for (const auto &v : keyboardFrame)
                    send(virtualKeys, v.type, v.code, v.value);
            keyboardFrame.clear();
            applyPending();
        }
        if (rc == LIBEVDEV_READ_STATUS_SYNC || (rc < 0 && rc != -EAGAIN)) {
            fprintf(stderr, "keyboard disconnected or event overflow\n");
            QCoreApplication::exit(1);
        }
    }
    void penEvents() {
        input_event e;
        int rc;
        while ((rc = libevdev_next_event(pen, LIBEVDEV_READ_FLAG_NORMAL, &e)) == 0) {
            frame.push_back(e);
            if (e.type != EV_SYN || e.code != SYN_REPORT)
                continue;
            bool oldTouch = touch;
            for (const auto &v : frame) {
                if (v.type == EV_ABS && v.code == ABS_X)
                    x = v.value;
                if (v.type == EV_ABS && v.code == ABS_Y)
                    y = v.value;
                if (v.type == EV_KEY && v.code == BTN_TOUCH)
                    touch = v.value;
                if (v.type == EV_KEY && v.code == BTN_TOOL_PEN)
                    proximity = v.value;
            }
            bool region = softEnabled && proximity && y < ymin + (ymax - ymin) * softHeight;
            if (region || !proximity) {
                penButtons.cancel(!proximity);
            }
            if (region && !inSoft) {
                send(virtualPen, EV_KEY, BTN_TOUCH, 0);
                send(virtualPen, EV_KEY, BTN_STYLUS, 0);
                if (libevdev_has_event_code(pen, EV_KEY, BTN_STYLUS2))
                    send(virtualPen, EV_KEY, BTN_STYLUS2, 0);
                send(virtualPen, EV_KEY, BTN_TOOL_PEN, 0);
                sync(virtualPen);
            }
            if (region && touch && !oldTouch) {
                int index = qBound(0, int((double(x - xmin) / (xmax - xmin)) * 16), 15);
                printf("soft %d\n", index);
                fflush(stdout);
                auto it = soft.find(index);
                if (it != soft.end()) {
                    activeSoft = it->second;
                    softDown = true;
                    action(activeSoft, true);
                }
            }
            if (softDown && (!touch || !region)) {
                action(activeSoft, false);
                softDown = false;
            }
            std::vector<input_event> filtered;
            for (const auto &v : frame) {
                if (v.type == EV_KEY && (v.code == BTN_STYLUS || v.code == BTN_STYLUS2)) {
                    auto it = penBindings.find(v.code);
                    auto d = penButtons.process(v.code, v.value, it != penBindings.end(),
                                                proximity && !region && !inSoft);
                    if (d.consume)
                        continue;
                }
                filtered.push_back(v);
            }
            if (!region) {
                if (inSoft && proximity) {
                    send(virtualPen, EV_ABS, ABS_X, x);
                    send(virtualPen, EV_ABS, ABS_Y, y);
                    for (int c : {ABS_PRESSURE, ABS_TILT_X, ABS_TILT_Y})
                        if (libevdev_has_event_code(pen, EV_ABS, c))
                            send(virtualPen, EV_ABS, c, libevdev_get_event_value(pen, EV_ABS, c));
                    send(virtualPen, EV_KEY, BTN_TOOL_PEN, 1);
                    send(virtualPen, EV_KEY, BTN_TOUCH, touch);
                    for (int code : {BTN_STYLUS, BTN_STYLUS2})
                        if (libevdev_has_event_code(pen, EV_KEY, code) && !penBindings.count(code))
                            send(virtualPen, EV_KEY, code,
                                 libevdev_get_event_value(pen, EV_KEY, code));
                    sync(virtualPen);
                } else
                    for (const auto &v : filtered)
                        send(virtualPen, v.type, v.code, v.value);
            }
            inSoft = region;
            frame.clear();
            applyPending();
        }
        if (rc == LIBEVDEV_READ_STATUS_SYNC || (rc < 0 && rc != -EAGAIN)) {
            fprintf(stderr, "pen disconnected or event overflow\n");
            QCoreApplication::exit(1);
        }
    }

public:
    bool start() {
        penFd = open(penPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        keyFd = open(keyPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (penFd < 0 || keyFd < 0 || libevdev_new_from_fd(penFd, &pen) < 0 ||
            libevdev_new_from_fd(keyFd, &keyboard) < 0) {
            perror("open tablet");
            return false;
        }
        if (libevdev_get_id_vendor(pen) != 0x256c || libevdev_get_id_product(pen) != 0x2004 ||
            libevdev_get_id_vendor(keyboard) != 0x256c ||
            libevdev_get_id_product(keyboard) != 0x2004) {
            fprintf(stderr, "unexpected device\n");
            return false;
        }
        if (!libevdev_has_event_code(pen, EV_ABS, ABS_X) ||
            !libevdev_has_event_code(pen, EV_ABS, ABS_Y))
            return false;
        xmin = libevdev_get_abs_minimum(pen, ABS_X);
        xmax = libevdev_get_abs_maximum(pen, ABS_X);
        ymin = libevdev_get_abs_minimum(pen, ABS_Y);
        ymax = libevdev_get_abs_maximum(pen, ABS_Y);
        if (xmax <= xmin || ymax <= ymin)
            return false;
        libevdev_set_name(pen, "GaomonManagePanel Virtual Pen");
        if (libevdev_uinput_create_from_device(pen, LIBEVDEV_UINPUT_OPEN_MANAGED, &virtualPen) <
            0) {
            perror("virtual pen");
            return false;
        }
        if (libevdev_new_from_fd(keyFd, &outputKeys) < 0)
            return false;
        libevdev_set_name(outputKeys, "GaomonManagePanel Shortcut Keyboard");
        libevdev_enable_event_type(outputKeys, EV_KEY);
        for (int i = 1; i <= KEY_MAX; i++)
            libevdev_enable_event_code(outputKeys, EV_KEY, i, nullptr);
        if (libevdev_uinput_create_from_device(outputKeys, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                               &virtualKeys) < 0) {
            perror("virtual keyboard");
            return false;
        }
        outputMouse = libevdev_new();
        libevdev_set_name(outputMouse, "GaomonManagePanel Pen Mouse Buttons");
        libevdev_set_id_bustype(outputMouse, BUS_USB);
        libevdev_enable_event_type(outputMouse, EV_REL);
        libevdev_enable_event_code(outputMouse, EV_REL, REL_X, nullptr);
        libevdev_enable_event_code(outputMouse, EV_REL, REL_Y, nullptr);
        libevdev_enable_event_type(outputMouse, EV_KEY);
        for (int code : {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE})
            libevdev_enable_event_code(outputMouse, EV_KEY, code, nullptr);
        if (libevdev_uinput_create_from_device(outputMouse, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                               &virtualMouse) < 0) {
            perror("virtual mouse");
            return false;
        }
        if (libevdev_grab(pen, LIBEVDEV_GRAB) < 0 || libevdev_grab(keyboard, LIBEVDEV_GRAB) < 0) {
            fprintf(stderr, "cannot grab tablet interfaces\n");
            return false;
        }
        // After opening the devices, the event loop has no reason to retain root.
        const char *uid = getenv("PKEXEC_UID");
        if (uid && geteuid() == 0) {
            auto user = static_cast<uid_t>(strtoul(uid, nullptr, 10));
            auto *pw = getpwuid(user);
            if (!pw || setgroups(0, nullptr) < 0 || setgid(pw->pw_gid) < 0 || setuid(user) < 0) {
                perror("drop privileges");
                return false;
            }
        }
        penNotify = new QSocketNotifier(penFd, QSocketNotifier::Read, this);
        connect(penNotify, &QSocketNotifier::activated, this, [this] { penEvents(); });
        keyNotify = new QSocketNotifier(keyFd, QSocketNotifier::Read, this);
        connect(keyNotify, &QSocketNotifier::activated, this, [this] { keyboardEvents(); });
        commands = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
        connect(commands, &QSocketNotifier::activated, this, [this] { readCommands(); });
        puts("ready");
        fflush(stdout);
        return true;
    }
    ~Helper() {
        if (virtualPen)
            libevdev_uinput_destroy(virtualPen);
        if (virtualKeys)
            libevdev_uinput_destroy(virtualKeys);
        if (virtualMouse)
            libevdev_uinput_destroy(virtualMouse);
        if (pen)
            libevdev_free(pen);
        if (keyboard)
            libevdev_free(keyboard);
        if (outputKeys)
            libevdev_free(outputKeys);
        if (outputMouse)
            libevdev_free(outputMouse);
        if (penFd >= 0)
            close(penFd);
        if (keyFd >= 0)
            close(keyFd);
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    Helper h;
    if (!h.start())
        return 1;
    return app.exec();
}
