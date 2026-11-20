#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
inline void drawCapsule(BYTE *bits, int width, int height, bool hot) {
    using namespace Gdiplus;
    Bitmap surface(width, height, width * 4, PixelFormat32bppPARGB, (BYTE *)bits);
    Graphics g(&surface);
    g.Clear(Color(0, 0, 0, 0));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.ScaleTransform(width / 36.f, height / 52.f);
    GraphicsPath path;
    path.AddArc(2, 2, 32, 32, 180, 180);
    path.AddArc(2, 18, 32, 32, 0, 180);
    path.CloseFigure();
    HIGHCONTRASTW hc{sizeof(hc)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0);
    bool contrast = hc.dwFlags & HCF_HIGHCONTRASTON;
    LinearGradientBrush glass(Point(1, 2), Point(34, 50),
                              contrast ? Color(255, 0, 0, 0) : Color(hot ? 251 : 231, 138, 159, 176),
                              contrast ? Color(255, 0, 0, 0) : Color(232, 33, 52, 73));
    g.FillPath(&glass, &path);
    Pen rim(Color(220, 245, 252, 255), 1.25f);
    g.DrawPath(&rim, &path);
    Pen ink(Color(255, 255, 255, 255), 1.9f);
    ink.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    g.DrawLine(&ink, 11, 21, 15, 25);
    g.DrawLine(&ink, 15, 25, 11, 29);
    g.DrawLine(&ink, 19, 29, 25, 29);
}
