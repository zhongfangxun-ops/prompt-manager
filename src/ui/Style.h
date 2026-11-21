#pragma once
#include "platform/App.h"
#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>
struct Palette {
    bool contrast = false;
    COLORREF paper = RGB(255, 255, 255), rail = RGB(244, 246, 253), bar = RGB(230, 235, 249),
             ink = RGB(29, 33, 46), muted = RGB(93, 101, 123), line = RGB(214, 219, 235),
             tint = RGB(222, 230, 255), accent = RGB(57, 84, 175), reverse = RGB(255, 255, 255);
    Palette() {
        HIGHCONTRASTW h{sizeof(h)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(h), &h, 0);
        if (h.dwFlags & HCF_HIGHCONTRASTON) {
            contrast = true;
            paper = rail = bar = GetSysColor(COLOR_WINDOW);
            ink = muted = GetSysColor(COLOR_WINDOWTEXT);
            line = ink;
            tint = paper;
            accent = GetSysColor(COLOR_HIGHLIGHT);
            reverse = GetSysColor(COLOR_HIGHLIGHTTEXT);
        }
    }
};
inline void fill(HDC dc, RECT r, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
}
inline Gdiplus::Color tone(COLORREF c, BYTE alpha = 255) {
    return {alpha, GetRValue(c), GetGValue(c), GetBValue(c)};
}
// Draw a continuous rounded outline, a tinted surface and its light-catching rim.
inline void surface(HDC dc, RECT r, float radius, Gdiplus::Color top, Gdiplus::Color bottom,
                    Gdiplus::Color rim) {
    using namespace Gdiplus;
    float x = r.left + 0.75f, y = r.top + 0.75f, w = r.right - r.left - 1.5f,
          h = r.bottom - r.top - 1.5f;
    if (w <= 1 || h <= 1)
        return;
    float d = std::min({2 * radius, w, h});
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    GraphicsPath shape;
    shape.AddArc(x, y, d, d, 180, 90);
    shape.AddArc(x + w - d, y, d, d, 270, 90);
    shape.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    shape.AddArc(x, y + h - d, d, d, 90, 90);
    shape.CloseFigure();
    LinearGradientBrush wash(PointF(x, y), PointF(x, y + h), top, bottom);
    g.FillPath(&wash, &shape);
    Pen edge(rim, 1.0f);
    g.DrawPath(&edge, &shape);
}
inline void rounded(HDC dc, RECT r, int radius, COLORREF bg, COLORREF line) {
    surface(dc, r, float(radius), tone(bg), tone(bg), tone(line));
}
inline HFONT fontAt(int px, int weight, UINT dpi) {
    return CreateFontW(-MulDiv(px, dpi, 96), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                       L"Microsoft YaHei UI");
}
inline void label(HDC dc, std::wstring_view text, RECT r, HFONT font, COLORREF color,
                  UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS) {
    auto old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.data(), int(text.size()), &r, flags | DT_NOPREFIX);
    SelectObject(dc, old);
}
inline std::wstring valueOf(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, 0);
    s.resize(GetWindowTextW(h, s.data(), n + 1));
    return s;
}
