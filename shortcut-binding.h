#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <linux/input-event-codes.h>
#include <optional>
#include <vector>

struct Binding {
    int key = 0;
    bool mouse = false;
    std::vector<int> modifiers;
};
inline std::optional<Binding> parseBinding(const QJsonObject &v) {
    Binding b;
    const QString kind = v.value("kind").toString(QStringLiteral("shortcut"));
    if (kind == "disabled")
        return b;
    if (kind == "left" || kind == "right" || kind == "middle") {
        b.mouse = true;
        b.key = kind == "left" ? BTN_LEFT : kind == "right" ? BTN_RIGHT : BTN_MIDDLE;
    } else if (kind == "shortcut") {
        b.key = v.value("key").toInt();
        if (b.key < 1 || b.key > KEY_MAX)
            return std::nullopt;
    } else
        return std::nullopt;
    for (auto m : v.value("modifiers").toArray()) {
        int n = m.toInt();
        if (n == KEY_LEFTCTRL || n == KEY_LEFTSHIFT || n == KEY_LEFTALT || n == KEY_LEFTMETA)
            b.modifiers.push_back(n);
    }
    return b;
}
