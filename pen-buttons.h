#pragma once
#include <linux/input.h>
#include <array>
#include <vector>

struct PenButtonDecision {
    bool consume = false, press = false, release = false;
};
class PenButtons {
    std::array<bool, 2> held{}, active{};
    static int index(int code) {
        return code == BTN_STYLUS ? 0 : code == BTN_STYLUS2 ? 1 : -1;
    }

public:
    bool busy() const {
        return held[0] || held[1];
    }
    PenButtonDecision process(int code, int value, bool mapped, bool allowAction) {
        int i = index(code);
        if (i < 0)
            return {};
        PenButtonDecision d;
        if (value == 1) {
            held[i] = true;
            if (mapped) {
                d.consume = true;
                if (allowAction && !active[i]) {
                    active[i] = true;
                    d.press = true;
                }
            }
        } else if (value == 0) {
            held[i] = false;
            d.consume = mapped || active[i];
            if (active[i]) {
                active[i] = false;
                d.release = true;
            }
        } else
            d.consume = mapped || active[i];
        return d;
    }
    std::vector<int> cancel(bool clearHeld) {
        std::vector<int> released;
        for (int i = 0; i < 2; i++) {
            if (active[i])
                released.push_back(i == 0 ? BTN_STYLUS : BTN_STYLUS2);
            active[i] = false;
            if (clearHeld)
                held[i] = false;
        }
        return released;
    }
};
