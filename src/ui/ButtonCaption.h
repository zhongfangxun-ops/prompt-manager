#pragma once
#include "ui/Style.h"
#include <string_view>

inline bool drawButtonCaption(HDC dc, RECT button, UINT dpi, COLORREF color, std::wstring_view caption,
                              const wchar_t *face) {
    using namespace Gdiplus;
    constexpr std::wstring_view prefix = L"＋ ";
    const bool hasPlus = caption.starts_with(prefix);
    if (hasPlus)
        caption.remove_prefix(prefix.size());
    const float scale = dpi / 96.f;
    FontFamily family(face);
    GraphicsPath text;
    if (family.GetLastStatus() != Ok ||
        text.AddString(caption.data(), int(caption.size()), &family, FontStyleRegular, 14.5f * scale,
                       PointF(0.f, 0.f), StringFormat::GenericTypographic()) != Ok)
        return false;
    RectF ink;
    if (text.GetBounds(&ink) != Ok || ink.Width <= 0 || ink.Height <= 0)
        return false;

    // Center the visible strokes of the caption and the plus together.
    const float plus = 8.f * scale, gap = 6.f * scale, stroke = 1.35f * scale;
    const int width = button.right - button.left, height = button.bottom - button.top;
    if (width <= 0 || height <= 0)
        return false;
    const float iconWidth = hasPlus ? plus + gap : 0.f;
    if (ink.Width + iconWidth > width - 4.f * scale)
        return false;
    const float left = (width - iconWidth - ink.Width) / 2.f, middle = height / 2.f;
    Matrix position;
    position.Translate(left + iconWidth - ink.X, middle - ink.Height / 2.f - ink.Y);
    text.Transform(&position);

    // Rasterize the outlines at four times the device resolution, then composite
    // their transparent edges directly onto the button's current gradient.
    constexpr int samples = 4;
    Bitmap layer(width * samples, height * samples, PixelFormat32bppPARGB);
    if (layer.GetLastStatus() != Ok)
        return false;
    {
        Graphics g(&layer);
        g.Clear(Color(0, 0, 0, 0));
        g.ScaleTransform(float(samples), float(samples));
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush brush(tone(color));
        Pen pen(tone(color), stroke);
        pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        const float center = left + plus / 2.f, arm = (plus - stroke) / 2.f;
        if (g.FillPath(&brush, &text) != Ok)
            return false;
        if (hasPlus && (g.DrawLine(&pen, center - arm, middle, center + arm, middle) != Ok ||
                        g.DrawLine(&pen, center, middle - arm, center, middle + arm) != Ok))
            return false;
    }
    Graphics target(dc);
    target.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    target.SetPixelOffsetMode(PixelOffsetModeHalf);
    return target.DrawImage(&layer, Rect(button.left, button.top, width, height), 0, 0, width * samples,
                            height * samples, UnitPixel) == Ok;
}
