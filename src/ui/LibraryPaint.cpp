#include "ui/LibraryWindow.h"
namespace {
std::wstring joined(const std::vector<std::wstring> &tags) {
    std::wstring s;
    for (auto &name : tags) {
        if (!s.empty())
            s += L"    /    ";
        s += name;
    }
    return s;
}
void backdrop(HDC dc, RECT bounds, const Palette &palette) {
    if (palette.contrast) {
        fill(dc, bounds, palette.paper);
        return;
    }
    using namespace Gdiplus;
    Graphics g(dc);
    float w = float(bounds.right), h = float(bounds.bottom);
    RectF area(0, 0, w, h);
    LinearGradientBrush sky(area, Color(255, 226, 235, 251), Color(255, 235, 241, 249), 32.f);
    g.FillRectangle(&sky, area);
    auto light = [&](float x, float y, float rx, float ry, Color glow) {
        GraphicsPath halo;
        halo.AddEllipse(x - rx, y - ry, 2 * rx, 2 * ry);
        PathGradientBrush brush(&halo);
        brush.SetCenterPoint(PointF(x, y));
        brush.SetCenterColor(glow);
        Color transparent(0, glow.GetR(), glow.GetG(), glow.GetB());
        int count = 1;
        brush.SetSurroundColors(&transparent, &count);
        g.FillPath(&brush, &halo);
    };
    light(.28f * w, .06f * h, .55f * w, .85f * h, Color(176, 194, 186, 244));
    light(.76f * w, -.12f * h, .49f * w, .88f * h, Color(190, 196, 235, 250));
}
void copyGlyph(HDC dc, RECT r, int unit, COLORREF color) {
    using namespace Gdiplus;
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    float scale = unit / 16.f, x = (r.left + r.right - unit) / 2.f, y = (r.top + r.bottom - unit) / 2.f;
    g.TranslateTransform(x, y);
    g.ScaleTransform(scale, scale);
    Pen pen(tone(color), 1.35f);
    pen.SetLineJoin(LineJoinRound);
    g.DrawRectangle(&pen, 5.f, 5.f, 9.f, 10.f);
    PointF back[] = {{3, 11}, {2, 11}, {2, 2}, {10, 2}, {10, 3}};
    g.DrawLines(&pen, back, 5);
}
} // namespace
void LibraryWindow::label(HDC dc, std::wstring_view text, RECT r, size_t index, COLORREF color,
                          UINT flags) const {
    if (text.empty() || r.right <= r.left || r.bottom <= r.top)
        return;
    using namespace Gdiplus;
    if (labelFonts[index]) {
        Graphics canvas(dc);
        canvas.SetPageUnit(UnitPixel);
        canvas.SetTextRenderingHint(TextRenderingHintAntiAlias);
        StringFormat format(StringFormatFlagsNoWrap);
        format.SetLineAlignment(StringAlignmentCenter);
        format.SetAlignment((flags & DT_CENTER) ? StringAlignmentCenter : StringAlignmentNear);
        format.SetTrimming((flags & DT_END_ELLIPSIS) ? StringTrimmingEllipsisCharacter
                                                     : StringTrimmingNone);
        SolidBrush ink(tone(color));
        RectF box(float(r.left), float(r.top), float(r.right - r.left), float(r.bottom - r.top));
        if (canvas.DrawString(text.data(), static_cast<INT>(text.size()), labelFonts[index].get(), box,
                              &format, &ink) == Ok)
            return;
    }
    auto old = SelectObject(dc, fonts[index]);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &r, flags | DT_NOPREFIX);
    SelectObject(dc, old);
}
void LibraryWindow::paintBackdrop(HDC dc, RECT bounds) {
    HDC memory = CreateCompatibleDC(dc);
    if (!memory) {
        backdrop(dc, bounds, colors);
        return;
    }
    bool refresh =
        !backdropBitmap || backdropSize.cx != bounds.right || backdropSize.cy != bounds.bottom;
    if (refresh) {
        DeleteObject(backdropBitmap);
        backdropBitmap = CreateCompatibleBitmap(dc, bounds.right, bounds.bottom);
        backdropSize = {bounds.right, bounds.bottom};
    }
    if (backdropBitmap) {
        auto old = SelectObject(memory, backdropBitmap);
        if (refresh)
            backdrop(memory, bounds, colors);
        BitBlt(dc, 0, 0, bounds.right, bounds.bottom, memory, 0, 0, SRCCOPY);
        SelectObject(memory, old);
    } else
        backdrop(dc, bounds, colors);
    DeleteDC(memory);
}
void LibraryWindow::paint() {
    PAINTSTRUCT ps;
    auto dc = BeginPaint(h, &ps);
    RECT bounds;
    GetClientRect(h, &bounds);
    paintBackdrop(dc, bounds);
    auto panel = [&](RECT r, COLORREF color) {
        if (!colors.contrast) {
            RECT shade = r;
            OffsetRect(&shade, 0, px(2));
            surface(dc, shade, px(23), tone(colors.muted, 26), tone(colors.muted, 18),
                    tone(colors.muted, 12));
        }
        rounded(dc, r, px(23), color, colors.contrast ? colors.line : RGB(255, 255, 255));
    };
    panel(rect(14, 57, split - 23, height - 72), colors.rail);
    panel(rect(split + 2, 57, width - split - 17, height - 72), colors.paper);
    auto frame = [&](int x, int y, int w, int ht) {
        rounded(dc, rect(x, y, w, ht), px(12), colors.paper, colors.line);
    };
    label(dc, L"提示词库", rect(106, 10, 110, 34), 1, colors.ink);
    frame(27, 83, split - 56, 42);
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.ScaleTransform(dpi / 96.f, dpi / 96.f);
        Gdiplus::Pen pen(tone(colors.muted), 1.55f);
        g.DrawEllipse(&pen, 38, 98, 10, 10);
        g.DrawLine(&pen, 47, 107, 51, 111);
    }
    auto count = L"提示词  ·  " + std::to_wstring(rows.size());
    label(dc, count, rect(36, 176, split - 70, 27), 1, colors.muted);
    int x = split + 36, rw = width - x - 43;
    auto badges = [&](const std::vector<std::wstring> &names, int y) {
        int left = x;
        auto prev = SelectObject(dc, fonts[0]);
        for (auto &name : names) {
            SIZE sz{};
            GetTextExtentPoint32W(dc, name.data(), int(name.size()), &sz);
            int available = x + rw - left;
            if (available < 35)
                break;
            int w = std::min(available, MulDiv(sz.cx, 96, dpi) + 24);
            RECT badge = rect(left, y, w, 24);
            rounded(dc, badge, px(12), colors.rail, colors.contrast ? colors.line : colors.rail);
            label(dc, name, badge, 0, colors.muted,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            left += w + 6;
        }
        SelectObject(dc, prev);
    };
    if (editing) {
        label(dc, L"标题", rect(x, 90, rw, 28), 1, colors.muted);
        frame(x, 121, rw, 42);
        label(dc, L"分类标签", rect(x, 173, rw, 25), 1, colors.muted);
        frame(x, 199, rw - 114, 36);
        label(dc, joined(editor.tags), rect(x, 236, rw, 27), 0, colors.accent);
        label(dc, L"正文", rect(x, 271, rw, 28), 1, colors.muted);
        frame(x, 300, rw, height - 335);
    } else if (current) {
        label(dc, current->title, rect(x, 99, rw, 47), 2, colors.ink);
        badges(current->tags, 149);
        fill(dc, rect(x, 182, rw, 1), colors.line);
    } else {
        bool filtered = category || !valueOf(item(Search)).empty();
        int cy = (height + 70) / 2;
        RECT mark = rect(x + rw / 2 - 25, cy - 68, 50, 50);
        surface(dc, mark, px(17), tone(colors.rail), tone(colors.tint), tone(colors.paper));
        copyGlyph(dc, mark, px(24), colors.accent);
        label(dc, filtered ? L"没有匹配的提示词" : L"还没有提示词", rect(x, cy - 8, rw, 36), 2,
              colors.ink, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (!notice.empty()) {
        int w = std::min(width - 430, 240);
        auto r = rect(185, 16, w, 30);
        rounded(dc, r, px(12), colors.tint, colors.line);
        label(dc, notice, r, 1, colors.accent, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    EndPaint(h, &ps);
}
bool LibraryWindow::draw(const DRAWITEMSTRUCT &d) {
    HDC dc = d.hDC;
    RECT r = d.rcItem;
    if (d.CtlID == List) {
        if (d.itemID >= rows.size())
            return true;
        auto &row = rows[d.itemID];
        bool selected = d.itemState & ODS_SELECTED;
        fill(dc, r, colors.rail);
        InflateRect(&r, -px(6), -px(3));
        if (selected) {
            surface(dc, r, px(12), tone(colors.paper), tone(colors.tint),
                    tone(colors.contrast ? colors.ink : colors.paper));
        }
        auto title = r;
        title.left += px(13);
        title.right -= px(55);
        label(dc, row.title, title, selected ? 1 : 0, colors.ink);
        auto action = r;
        action.left = action.right - px(43);
        if (copied == row.id)
            label(dc, L"已复制", action, 0, colors.accent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        else
            copyGlyph(dc, action, px(15), selected ? colors.accent : colors.muted);
        if (d.itemState & ODS_FOCUS) {
            InflateRect(&r, -px(2), -px(2));
            DrawFocusRect(dc, &r);
        }
        return true;
    }
    if (d.CtlType != ODT_BUTTON)
        return false;
    int id = d.CtlID;
    bool tag = id == FilterAll || id == FilterMore || id >= FilterFirst;
    bool selected = (id == FilterAll && category == 0);
    if (id >= FilterFirst && id < FilterFirst + (int)tags.size())
        selected = tags[id - FilterFirst].id == category;
    if (id == FilterMore && category) {
        auto it = std::find_if(tags.begin(), tags.end(), [&](auto &t) { return t.id == category; });
        selected = it != tags.end() && it - tags.begin() >= visibleTags;
    }
    bool primary = id == New || id == Save || id == EmptyAction;
    if (tag || id == TagMenu || id == EmptyAction) {
        fill(dc, r, tag ? colors.rail : colors.paper);
    } else {
        // Match the surrounding glass canvas across each native child window.
        POINT origin{};
        MapWindowPoints(d.hwndItem, h, &origin, 1);
        int state = SaveDC(dc);
        SetViewportOrgEx(dc, -origin.x, -origin.y, nullptr);
        RECT bounds;
        GetClientRect(h, &bounds);
        paintBackdrop(dc, bounds);
        RestoreDC(dc, state);
    }
    if (id == CloseWindowButton || id == MinimizeWindowButton || id == ZoomWindowButton) {
        using namespace Gdiplus;
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.ScaleTransform(dpi / 96.f, dpi / 96.f);
        COLORREF color = id == CloseWindowButton      ? RGB(241, 110, 106)
                         : id == MinimizeWindowButton ? RGB(245, 192, 82)
                                                      : RGB(99, 193, 129);
        SolidBrush brush(tone(colors.contrast ? colors.paper : color));
        Pen rim(tone(colors.contrast ? colors.ink : color), 1.f);
        g.FillEllipse(&brush, 5, 9, 14, 14);
        g.DrawEllipse(&rim, 5, 9, 14, 14);
        Pen ink(tone(colors.contrast ? colors.ink : RGB(65, 62, 65)), 1.15f);
        if (id == CloseWindowButton) {
            g.DrawLine(&ink, 9, 13, 15, 19);
            g.DrawLine(&ink, 15, 13, 9, 19);
        } else if (id == MinimizeWindowButton)
            g.DrawLine(&ink, 8, 16, 16, 16);
        else
            g.DrawRectangle(&ink, 9, 13, 6, 6);
        if (d.itemState & ODS_FOCUS)
            DrawFocusRect(dc, &r);
        return true;
    }
    InflateRect(&r, -1, -1);
    bool pressed = (d.itemState & ODS_SELECTED) != 0;
    if (primary) {
        COLORREF top = colors.contrast || pressed ? colors.accent : RGB(101, 132, 236);
        surface(dc, r, px(18), tone(top), tone(colors.accent),
                tone(colors.contrast ? colors.line : colors.paper));
    } else if (selected || !tag || pressed) {
        COLORREF base = selected || pressed ? colors.tint : colors.rail;
        surface(dc, r, px(18), tone(colors.paper, colors.contrast ? 255 : 205),
                tone(base, colors.contrast ? 255 : 112),
                tone(colors.contrast ? colors.line : colors.paper));
    }
    label(dc, valueOf(d.hwndItem), r, primary || selected ? 1 : 0,
          primary ? colors.reverse : (tag && !selected ? colors.muted : colors.ink),
          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (d.itemState & ODS_FOCUS) {
        InflateRect(&r, -px(3), -px(3));
        DrawFocusRect(dc, &r);
    }
    return true;
}
