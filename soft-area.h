#pragma once
#include <QRectF>
#include <algorithm>

// Based on the 1060PRO vendor layout, expressed in physical input coordinates.
inline constexpr double softAreaHeight = 15.0 / 356.0;
inline QRectF physicalInputArea(const QRectF &display, bool reserveSoft) {
    if (!reserveSoft)
        return display;
    return {display.x(), softAreaHeight + display.y() * (1 - softAreaHeight), display.width(),
            display.height() * (1 - softAreaHeight)};
}
inline QRectF displayInputArea(const QRectF &physical, bool reserveSoft) {
    if (!reserveSoft)
        return physical;
    const double top = std::clamp(physical.top(), softAreaHeight, 1.0);
    const double bottom = std::clamp(physical.bottom(), softAreaHeight, 1.0);
    // An entirely reserved selection has no drawable intersection.
    if (bottom <= top)
        return {physical.x(), 0, physical.width(), 1};
    return {physical.x(), (top - softAreaHeight) / (1 - softAreaHeight), physical.width(),
            (bottom - top) / (1 - softAreaHeight)};
}
