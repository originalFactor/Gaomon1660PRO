#include "../keyboard-matcher.h"
#include <cassert>
input_event event(int code, int value) {
    input_event e{};
    e.type = EV_KEY;
    e.code = code;
    e.value = value;
    return e;
}
int main() {
    KeyboardMatcher m;
    auto mapping = [](int code, int mods) {
        return (code == KEY_S && mods == 1) ||
               ((code == KEY_A || code == KEY_B || code == KEY_E) && mods == 0);
    };
    auto d = m.process({event(KEY_LEFTCTRL, 1), event(KEY_S, 1)}, mapping);
    assert(d.consume && d.presses.size() == 1 && d.presses[0] == KEY_S && d.releases.empty() &&
           m.busy());
    d = m.process({event(KEY_S, 0), event(KEY_LEFTCTRL, 0)}, mapping);
    assert(d.consume && d.presses.empty() && d.releases.size() == 1 && !m.busy() &&
           m.currentModifiers() == 0);
    d = m.process({event(KEY_LEFTCTRL, 1), event(KEY_A, 1)}, mapping);
    assert(!d.consume && d.presses.empty() && !m.busy());
    d = m.process({event(KEY_A, 0), event(KEY_LEFTCTRL, 0)}, mapping);
    assert(!d.consume && m.currentModifiers() == 0);
    d = m.process({event(KEY_B, 1)}, mapping);
    assert(d.consume && d.presses.size() == 1 && d.presses[0] == KEY_B);
    d = m.process({event(KEY_E, 1)}, mapping);
    assert(d.consume && d.presses.size() == 1 && d.presses[0] == KEY_E);
    d = m.process({event(KEY_B, 0)}, mapping);
    assert(d.consume && d.releases.size() == 1 && d.releases[0] == KEY_B && m.busy());
    d = m.process({event(KEY_E, 0)}, mapping);
    assert(d.consume && d.releases.size() == 1 && d.releases[0] == KEY_E && !m.busy());
}
