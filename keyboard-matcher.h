#pragma once
#include <linux/input.h>
#include <functional>
#include <set>
#include <vector>

// A physical HID keyboard report is delivered as a SYN_REPORT frame.
struct KeyDecision {
    bool consume = false;
    std::vector<int> presses, releases;
};
class KeyboardMatcher {
    int modifiers = 0;
    std::set<int> active;
    static int modifierBit(int code) {
        switch (code) {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            return 1;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            return 2;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            return 4;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            return 8;
        default:
            return 0;
        }
    }

public:
    bool busy() const {
        return !active.empty();
    }
    KeyDecision process(const std::vector<input_event> &frame,
                        const std::function<bool(int, int)> &mapped) {
        std::vector<int> downs, ups;
        for (const auto &v : frame)
            if (v.type == EV_KEY) {
                int bit = modifierBit(v.code);
                if (bit) {
                    if (v.value)
                        modifiers |= bit;
                    else
                        modifiers &= ~bit;
                } else if (v.value == 1)
                    downs.push_back(v.code);
                else if (v.value == 0)
                    ups.push_back(v.code);
            }
        KeyDecision d;
        d.consume = !active.empty();
        for (int code : downs)
            if (!active.count(code) && mapped(code, modifiers)) {
                active.insert(code);
                d.consume = true;
                d.presses.push_back(code);
            }
        for (int code : ups)
            if (active.erase(code)) {
                d.consume = true;
                d.releases.push_back(code);
            }
        return d;
    }
    int currentModifiers() const {
        return modifiers;
    }
};
