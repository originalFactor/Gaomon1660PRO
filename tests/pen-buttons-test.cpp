#include "../pen-buttons.h"
#include <cassert>
int main() {
    PenButtons pen;
    auto a = pen.process(BTN_STYLUS, 1, false, true);
    assert(!a.consume && !a.press && pen.busy());
    a = pen.process(BTN_STYLUS, 0, false, true);
    assert(!a.consume && !a.release && !pen.busy());
    a = pen.process(BTN_STYLUS, 1, true, true);
    assert(a.consume && a.press && pen.busy());
    a = pen.process(BTN_STYLUS, 0, true, true);
    assert(a.consume && a.release && !pen.busy());
    a = pen.process(BTN_STYLUS2, 1, true, false);
    assert(a.consume && !a.press && pen.busy());
    a = pen.process(BTN_STYLUS2, 0, true, false);
    assert(a.consume && !a.release && !pen.busy());
    pen.process(BTN_STYLUS, 1, true, true);
    pen.process(BTN_STYLUS2, 1, true, true);
    auto cancelled = pen.cancel(true);
    assert(cancelled.size() == 2 && !pen.busy());
    a = pen.process(BTN_STYLUS, 0, true, true);
    assert(a.consume && !a.release);
}
