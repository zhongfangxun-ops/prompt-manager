#include "ui/LibraryWindow.h"
#include <dwmapi.h>
#include <oleacc.h>

namespace {
// Windows accessibility reads this prefix for owner-drawn menu items.
struct MenuRow : MSAAMENUINFO {
    std::wstring text;
    bool separator = false;
};

MenuRow &rowFrom(ULONG_PTR data) {
    return *static_cast<MenuRow *>(reinterpret_cast<MSAAMENUINFO *>(data));
}

std::wstring displayText(HMENU menu, int pos) {
    int len = GetMenuStringW(menu, pos, nullptr, 0, MF_BYPOSITION);
    std::wstring raw(len + 1, 0);
    raw.resize(GetMenuStringW(menu, pos, raw.data(), len + 1, MF_BYPOSITION));
    for (size_t i = 0; (i = raw.find(L"&&", i)) != raw.npos; ++i)
        raw.erase(i, 1);
    return raw;
}
} // namespace

int LibraryWindow::trackPopup(int control, HMENU menu) {
    const int count = GetMenuItemCount(menu);
    if (count <= 0)
        return 0;
    TPMPARAMS anchor{sizeof(anchor)};
    GetWindowRect(item(control), &anchor.rcExclude);
    InflateRect(&anchor.rcExclude, 0, px(4));
    auto open = [&] {
        return TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_WORKAREA,
                                anchor.rcExclude.left, anchor.rcExclude.bottom, h, &anchor);
    };
    if (colors.contrast)
        return open();

    std::vector<MenuRow> rows(count);
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW info{sizeof(info)};
        info.fMask = MIIM_FTYPE;
        GetMenuItemInfoW(menu, i, TRUE, &info);
        auto &row = rows[i];
        row.separator = (info.fType & MFT_SEPARATOR) != 0;
        row.text = displayText(menu, i);
        row.dwMSAASignature = MSAA_MENU_SIG;
        row.cchWText = static_cast<DWORD>(row.text.size());
        row.pszWText = row.text.data();
        info.fMask |= MIIM_DATA;
        info.fType |= MFT_OWNERDRAW;
        info.dwItemData = reinterpret_cast<ULONG_PTR>(static_cast<MSAAMENUINFO *>(&row));
        SetMenuItemInfoW(menu, i, TRUE, &info);
    }
    HBRUSH background = CreateSolidBrush(colors.paper);
    MENUINFO style{sizeof(style)};
    style.fMask = MIM_BACKGROUND | MIM_STYLE;
    style.hbrBack = background;
    style.dwStyle = MNS_NOCHECK;
    SetMenuInfo(menu, &style);
    activePopup = menu;
    int command = open();
    activePopup = nullptr;
    styledPopup = nullptr;
    DeleteObject(background);
    return command;
}

bool LibraryWindow::measurePopup(MEASUREITEMSTRUCT &d) {
    if (!activePopup || d.CtlType != ODT_MENU || !d.itemData)
        return false;
    auto &row = rowFrom(d.itemData);
    d.itemHeight = px(row.separator ? 9 : 35);
    SIZE size{};
    HDC dc = GetDC(h);
    auto old = SelectObject(dc, fonts[serifIndex(0, row.text)]);
    GetTextExtentPoint32W(dc, row.text.c_str(), static_cast<int>(row.text.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(h, dc);
    d.itemWidth = std::clamp<int>(size.cx + px(56), px(156), px(360));
    return true;
}

bool LibraryWindow::drawPopup(const DRAWITEMSTRUCT &d) {
    if (!activePopup || d.CtlType != ODT_MENU || (HMENU)d.hwndItem != activePopup || !d.itemData)
        return false;
    const auto &row = rowFrom(d.itemData);
    RECT r = d.rcItem;
    fill(d.hDC, r, colors.paper);
    if (row.separator) {
        InflateRect(&r, -px(13), 0);
        r.top = (r.top + r.bottom) / 2;
        r.bottom = r.top + px(1);
        fill(d.hDC, r, colors.line);
        return true;
    }
    const bool disabled = d.itemState & (ODS_DISABLED | ODS_GRAYED);
    if (!disabled && (d.itemState & ODS_SELECTED)) {
        RECT highlight = r;
        InflateRect(&highlight, -px(4), -px(2));
        rounded(d.hDC, highlight, px(9), colors.tint, colors.tint);
    }
    if (d.itemState & ODS_CHECKED) {
        using namespace Gdiplus;
        Graphics g(d.hDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        Pen ink(tone(colors.accent), 1.7f * dpi / 96.f);
        ink.SetLineJoin(LineJoinRound);
        ink.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        int x = r.right - px(24), y = (r.top + r.bottom) / 2;
        Point marks[] = {{x, y}, {x + px(3), y + px(3)}, {x + px(10), y - px(4)}};
        g.DrawLines(&ink, marks, 3);
    }
    r.left += px(13);
    r.right -= px(31);
    label(d.hDC, row.text, r, 0, disabled ? colors.muted : colors.ink);
    return true;
}

LRESULT LibraryWindow::popupChar(wchar_t key) {
    int first = -1, after = -1, matches = 0;
    bool passedHighlight = false;
    for (int i = 0; i < GetMenuItemCount(activePopup); ++i) {
        MENUITEMINFOW info{sizeof(info)};
        info.fMask = MIIM_STATE | MIIM_DATA;
        if (!GetMenuItemInfoW(activePopup, i, TRUE, &info) || !info.dwItemData ||
            (info.fState & MFS_DISABLED))
            continue;
        const auto &s = rowFrom(info.dwItemData).text;
        if (!s.empty() && CompareStringOrdinal(s.data(), 1, &key, 1, TRUE) == CSTR_EQUAL) {
            if (first < 0)
                first = i;
            if (passedHighlight && after < 0)
                after = i;
            ++matches;
        }
        passedHighlight |= (info.fState & MFS_HILITE) != 0;
    }
    return matches ? MAKELRESULT(after >= 0 ? after : first, matches == 1 ? MNC_EXECUTE : MNC_SELECT)
                   : MAKELRESULT(0, MNC_IGNORE);
}

void LibraryWindow::stylePopup(HWND popup) {
    if (!popup || styledPopup == popup)
        return;
    styledPopup = popup;
    BOOL dark = colors.dark;
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(popup, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(popup, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DwmSetWindowAttribute(popup, DWMWA_BORDER_COLOR, &colors.line, sizeof(colors.line));
}
