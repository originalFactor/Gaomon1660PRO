#include "action-state.h"
#include <cassert>
#include <tuple>
#include <vector>
int main() {
    using E = std::tuple<bool, int, int>;
    std::vector<E> events;
    ActionState state;
    auto dispatch = [&](bool mouse, int code, int value) {
        events.emplace_back(mouse, code, value);
    };
    auto sync = [](bool) {};
    Binding mouse;
    mouse.mouse = true;
    mouse.key = BTN_LEFT;
    mouse.modifiers = {KEY_LEFTCTRL};
    Binding keyboard;
    keyboard.key = KEY_Z;
    keyboard.modifiers = {KEY_LEFTCTRL};
    state.apply(mouse, true, dispatch, sync);
    state.apply(keyboard, true, dispatch, sync);
    state.apply(mouse, false, dispatch, sync);
    state.apply(keyboard, false, dispatch, sync);
    std::vector<E> expected = {{false, KEY_LEFTCTRL, 1}, {true, BTN_LEFT, 1},
                               {false, KEY_Z, 1},        {true, BTN_LEFT, 0},
                               {false, KEY_Z, 0},        {false, KEY_LEFTCTRL, 0}};
    assert(events == expected);
    events.clear();
    Binding disabled;
    state.apply(disabled, true, dispatch, sync);
    state.apply(disabled, false, dispatch, sync);
    assert(events.empty());
}
