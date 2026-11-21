#pragma once
#include "platform/App.h"
#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>
struct Palette {
    bool contrast = false, dark = false;
    COLORREF paper = RGB(255, 255, 255), rail = RGB(244, 246, 253), bar = RGB(230, 235, 249),
             ink = RGB(29, 33, 46), muted = RGB(93, 101, 123), line = RGB(214, 219, 235),
             tint = RGB(222, 230, 255), accent = RGB(57, 84, 175), reverse = RGB(255, 255, 255),
             accentTop = RGB(71, 99, 192), rim = RGB(255, 255, 255), shadow = RGB(93, 101, 123);
    explicit Palette(bool useDark = false) {
        HIGHCONTRASTW h{sizeof(h)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(h), &h, 0);
        if (h.dwFlags & HCF_HIGHCONTRASTON) {
            contrast = true;
            paper = rail = bar = GetSysColor(COLOR_WINDOW);
            ink = muted = GetSysColor(COLOR_WINDOWTEXT);
            line = rim = ink;
            tint = paper;
            accentTop = accent = GetSysColor(COLOR_HIGHLIGHT);
            reverse = GetSysColor(COLOR_HIGHLIGHTTEXT);
        } else if (useDark) {
            dark = true;
            paper = RGB(33, 39, 53);
            rail = RGB(28, 34, 48);
            bar = RGB(27, 33, 47);
            ink = RGB(237, 241, 249);
            muted = RGB(179, 191, 214);
            line = RGB(77, 89, 113);
            rim = RGB(64, 74, 97);
            tint = RGB(52, 68, 100);
            accent = RGB(167, 190, 255);
            accentTop = RGB(191, 209, 255);
            reverse = RGB(23, 36, 69);
            shadow = RGB(0, 0, 0);
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
                       OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH,
                       L"Microsoft YaHei UI");
}
inline std::wstring valueOf(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, 0);
    s.resize(GetWindowTextW(h, s.data(), n + 1));
    return s;
}
