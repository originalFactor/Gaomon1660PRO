#pragma once
#include "shortcut-binding.h"
#include <functional>
#include <map>

// Counts shared keys so overlapping bindings do not release a held modifier.
class ActionState {
    std::map<int, int> heldKeys, heldMouse;

public:
    using Emit = std::function<void(bool, int, int)>;
    using Sync = std::function<void(bool)>;
    void apply(const Binding &b, bool down, const Emit &dispatch, const Sync &sync) {
        if (!b.key)
            return;
        auto sendHeld = [&](bool mouse, int code, bool pressed) {
            auto &held = mouse ? heldMouse : heldKeys;
            if (pressed) {
                if (held[code]++ == 0)
                    dispatch(mouse, code, 1);
            } else {
                auto it = held.find(code);
                if (it != held.end() && --it->second == 0) {
                    dispatch(mouse, code, 0);
                    held.erase(it);
                }
            }
        };
        if (b.mouse) {
            if (down) {
                for (int m : b.modifiers)
                    sendHeld(false, m, true);
                if (!b.modifiers.empty())
                    sync(false);
            }
            sendHeld(true, b.key, down);
            sync(true);
            if (!down) {
                for (auto i = b.modifiers.rbegin(); i != b.modifiers.rend(); ++i)
                    sendHeld(false, *i, false);
                if (!b.modifiers.empty())
                    sync(false);
            }
        } else {
            if (down) {
                for (int m : b.modifiers)
                    sendHeld(false, m, true);
                sendHeld(false, b.key, true);
            } else {
                sendHeld(false, b.key, false);
                for (auto i = b.modifiers.rbegin(); i != b.modifiers.rend(); ++i)
                    sendHeld(false, *i, false);
            }
            sync(false);
        }
    }
};
