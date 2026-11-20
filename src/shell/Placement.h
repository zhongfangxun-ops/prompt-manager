#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
struct Anchor {
    int side = 1, fraction = 5000;
};
inline bool shouldShow(bool visible, bool fullscreen) { return visible && !fullscreen; }
inline POINT resolveAnchor(RECT r, Anchor a, int w, int h) {
    int dx = std::max(0L, r.right - r.left - w), dy = std::max(0L, r.bottom - r.top - h);
    int f = std::clamp(a.fraction, 0, 10000);
    if (a.side < 2)
        return {r.left + (a.side == 1 ? dx : 0), r.top + MulDiv(dy, f, 10000)};
    return {r.left + MulDiv(dx, f, 10000), r.top + (a.side == 3 ? dy : 0)};
}
inline Anchor nearestAnchor(RECT r, POINT pos, int w, int h) {
    int dx = std::max(1L, r.right - r.left - w), dy = std::max(1L, r.bottom - r.top - h);
    int x = std::clamp(int(pos.x - r.left), 0, dx), y = std::clamp(int(pos.y - r.top), 0, dy);
    std::array<int, 4> dist{x, dx - x, y, dy - y};
    int side = std::min_element(dist.begin(), dist.end()) - dist.begin();
    return {side, side < 2 ? MulDiv(y, 10000, dy) : MulDiv(x, 10000, dx)};
}

inline bool coversMonitor(RECT r, RECT m, LONG_PTR style, bool zoomed) {
    const bool framed = (style & WS_CAPTION) == WS_CAPTION;
    const bool extendedFrame =
        r.left < m.left - 1 || r.right > m.right + 1 || r.top < m.top - 1 || r.bottom > m.bottom + 1;
    if (framed && (zoomed || extendedFrame))
        return false;
    return r.left <= m.left && r.right >= m.right && r.top <= m.top && r.bottom >= m.bottom;
}
