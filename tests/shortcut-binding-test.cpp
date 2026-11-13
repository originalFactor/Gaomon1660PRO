#include "shortcut-binding.h"
#include <cassert>
int main() {
    auto keyboard =
        parseBinding(QJsonObject{{"key", KEY_Z}, {"modifiers", QJsonArray{KEY_LEFTCTRL}}});
    assert(keyboard && !keyboard->mouse && keyboard->key == KEY_Z &&
           keyboard->modifiers.size() == 1);
    auto mouse = parseBinding(
        QJsonObject{{"kind", "right"}, {"modifiers", QJsonArray{KEY_LEFTSHIFT, KEY_LEFTCTRL}}});
    assert(mouse && mouse->mouse && mouse->key == BTN_RIGHT && mouse->modifiers.size() == 2);
    auto disabled = parseBinding(QJsonObject{{"kind", "disabled"}});
    assert(disabled && !disabled->key);
    assert(!parseBinding(QJsonObject{{"kind", "shortcut"}, {"key", 0}}));
    assert(!parseBinding(QJsonObject{{"kind", "unknown"}}));
}
