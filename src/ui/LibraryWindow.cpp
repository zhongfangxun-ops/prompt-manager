#include "ui/LibraryWindow.h"
#include "platform/Clipboard.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <shellscalingapi.h>
namespace {
constexpr wchar_t className[] = L"PromptManager.Library";
LRESULT CALLBACK rowMouse(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto &view = *reinterpret_cast<LibraryWindow *>(data);
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
        RECT r;
        GetClientRect(h, &r);
        int x = (short)LOWORD(l);
        if (x >= r.right - view.px(57)) {
            LRESULT hit = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
            if (!HIWORD(hit) && LOWORD(hit) < view.rows.size()) {
                RECT actual{};
                SendMessageW(h, LB_GETITEMRECT, LOWORD(hit), (LPARAM)&actual);
                POINT point{x, (short)HIWORD(l)};
                if (!view.rowGesture.accepts(msg, point, actual, GetTickCount(), GetDoubleClickTime(),
                                             GetSystemMetrics(SM_CXDOUBLECLK),
                                             GetSystemMetrics(SM_CYDOUBLECLK)))
                    return 0;
                try {
                    view.copy(view.rows[LOWORD(hit)].id);
                } catch (const std::exception &e) {
                    view.flash(wideError(e));
                }
                return 0;
            }
        }
    }
    return DefSubclassProc(h, msg, w, l);
}
LRESULT CALLBACK inputComposition(HWND control, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto &view = *reinterpret_cast<LibraryWindow *>(data);
    if ((msg == WM_PAINT || msg == WM_PRINTCLIENT) && view.colors.dark &&
        !GetWindowTextLengthW(control)) {
        wchar_t cue[64]{};
        if (SendMessageW(control, EM_GETCUEBANNER, (WPARAM)cue, ARRAYSIZE(cue)) && *cue) {
            PAINTSTRUCT ps{};
            HDC dc = msg == WM_PAINT ? BeginPaint(control, &ps) : (HDC)w;
            RECT r;
            GetClientRect(control, &r);
            fill(dc, r, view.colors.paper);
            r.left += LOWORD(SendMessageW(control, EM_GETMARGINS, 0, 0));
            view.label(dc, cue, r, 0, view.colors.muted);
            if (msg == WM_PAINT)
                EndPaint(control, &ps);
            return 0;
        }
    }
    if (msg == WM_IME_STARTCOMPOSITION)
        view.composing = control;
    if ((msg == WM_IME_ENDCOMPOSITION || msg == WM_KILLFOCUS) && view.composing == control)
        view.composing = nullptr;
    return DefSubclassProc(control, msg, w, l);
}
LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    auto *v = reinterpret_cast<LibraryWindow *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        v = static_cast<LibraryWindow *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        v->h = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)v);
    }
    if (!v)
        return DefWindowProcW(h, msg, w, l);
    try {
        switch (msg) {
        case WM_NCCALCSIZE: {
            // Keep the resize frame and maximized work area supplied by Windows.
            auto client = reinterpret_cast<RECT *>(l);
            const LONG top = client->top;
            DefWindowProcW(h, msg, w, l);
            UINT dpi = GetDpiForWindow(h);
            int inset = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                        GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            client->top = top + (IsZoomed(h) ? inset : 0);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            ScreenToClient(h, &pt);
            RECT bounds;
            GetClientRect(h, &bounds);
            if (pt.y >= 0 && pt.y < v->px(5) && !IsZoomed(h)) {
                if (pt.x < v->px(7))
                    return HTTOPLEFT;
                if (pt.x >= bounds.right - v->px(7))
                    return HTTOPRIGHT;
                return HTTOP;
            }
            if (PtInRect(&bounds, pt)) {
                if (pt.y < v->px(57) && ChildWindowFromPointEx(h, pt, CWP_SKIPINVISIBLE) == h)
                    return HTCAPTION;
                return HTCLIENT;
            }
            return DefWindowProcW(h, msg, w, l);
        }
        case WM_CREATE:
            v->create();
            return 0;
        case WM_SIZE:
            if (v->ready && (v->width != MulDiv(LOWORD(l), 96, v->dpi) ||
                             v->height != MulDiv(HIWORD(l), 96, v->dpi)))
                v->layout();
            return 0;
        case WM_GETMINMAXINFO: {
            auto m = reinterpret_cast<MINMAXINFO *>(l);
            auto mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{sizeof(mi)};
            GetMonitorInfoW(mon, &mi);
            m->ptMinTrackSize = {std::min(v->px(650), int(mi.rcWork.right - mi.rcWork.left)),
                                 std::min(v->px(460), int(mi.rcWork.bottom - mi.rcWork.top))};
            return 0;
        }
        case WM_DPICHANGED: {
            v->dpi = HIWORD(w);
            v->style();
            auto r = (RECT *)l;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER);
            v->layout();
            return 0;
        }
        case WM_SYSCOLORCHANGE:
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (v->ready) {
                v->style();
                v->layout();
                RedrawWindow(h, nullptr, nullptr,
                             RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            v->paint();
            return 0;
        case WM_DRAWITEM:
            if (v->drawPopup(*(DRAWITEMSTRUCT *)l))
                return TRUE;
            if (v->draw(*(DRAWITEMSTRUCT *)l))
                return TRUE;
            break;
        case WM_MEASUREITEM:
            if (v->measurePopup(*(MEASUREITEMSTRUCT *)l))
                return TRUE;
            break;
        case WM_MENUCHAR:
            if (v->activePopup && v->activePopup == (HMENU)l)
                return v->popupChar(LOWORD(w));
            break;
        case WM_ENTERIDLE:
            if (w == MSGF_MENU && v->activePopup)
                v->stylePopup((HWND)l);
            break;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            auto dc = (HDC)w;
            auto id = GetDlgCtrlID((HWND)l);
            bool rail = id == List;
            SetTextColor(dc, v->colors.ink);
            SetBkColor(dc, rail ? v->colors.rail : v->colors.paper);
            return (LRESULT)(rail ? v->railBrush : v->paperBrush);
        }
        case WM_COMMAND:
            if (HIWORD(w) == EN_CHANGE && v->colors.dark &&
                (LOWORD(w) == Search || LOWORD(w) == TagInput))
                InvalidateRect((HWND)l, nullptr, FALSE);
            if (HIWORD(w) == BN_CLICKED) {
                switch (LOWORD(w)) {
                case CloseWindowButton:
                    PostMessageW(h, WM_CLOSE, 0, 0);
                    return 0;
                case MinimizeWindowButton:
                    ShowWindow(h, SW_MINIMIZE);
                    return 0;
                case ZoomWindowButton:
                    ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE);
                    return 0;
                }
            }
            if (v->ready && !v->loading)
                v->command(LOWORD(w), HIWORD(w));
            return 0;
        case WM_TIMER:
            if (w == 9) {
                KillTimer(h, 9);
                v->notice.clear();
                v->copied = 0;
                InvalidateRect(h, nullptr, FALSE);
                InvalidateRect(v->item(List), nullptr, FALSE);
            }
            return 0;
        case WM_SETFOCUS:
            if (v->ready)
                SetFocus(v->item(v->editing ? TitleInput : Search));
            return 0;
        case WM_CLOSE:
            if (!v->editing || v->leaveEdit())
                DestroyWindow(h);
            return 0;
        case WM_NCDESTROY:
            // The shell can shut down GDI+ before releasing the library's C++ state.
            v->labelFonts = {};
            PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return DefWindowProcW(h, msg, w, l);
        }
    } catch (const std::exception &e) {
        MessageBoxW(h, wideError(e).c_str(), L"提示词", MB_ICONERROR);
        if (msg == WM_CREATE)
            return -1;
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}
std::unique_ptr<LibraryWindow> active;
} // namespace
LibraryWindow::~LibraryWindow() {
    DeleteObject(backdropBitmap);
    for (auto f : fonts)
        DeleteObject(f);
    DeleteObject(paperBrush);
    DeleteObject(railBrush);
}
void LibraryWindow::create() {
    dpi = GetDpiForWindow(h);
    auto add = [&](int id, const wchar_t *cls, const wchar_t *title, DWORD style) {
        HWND c = CreateWindowExW(0, cls, title, WS_CHILD | WS_TABSTOP | style, 0, 0, 10, 10, h,
                                 (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
        if (!c)
            throw std::runtime_error("无法创建窗口控件。");
        if (!wcscmp(cls, L"EDIT")) {
            SetWindowSubclass(c, inputComposition, 1, (DWORD_PTR)this);
            SendMessageW(c, EM_SETLIMITTEXT, 0x7ffffffe, 0);
            SendMessageW(c, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(2, 2));
        }
        return c;
    };
    auto btn = [&](int id, const wchar_t *s) { return add(id, L"BUTTON", s, BS_OWNERDRAW); };
    add(Search, L"EDIT", L"", ES_AUTOHSCROLL);
    SendMessageW(item(Search), EM_SETCUEBANNER, TRUE, (LPARAM)L"搜索标题或正文");
    btn(CloseWindowButton, L"关闭");
    btn(MinimizeWindowButton, L"最小化");
    btn(ZoomWindowButton, L"最大化");
    btn(FilterAll, L"全部");
    for (int i = 0; i < 4; i++)
        btn(FilterFirst + i, L"");
    btn(FilterMore, L"更多分类");
    auto list = add(List, L"LISTBOX", L"提示词列表",
                    LBS_NODATA | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT | WS_VSCROLL);
    SetWindowSubclass(list, rowMouse, 1, (DWORD_PTR)this);
    btn(New, L"＋ 新建");
    btn(Edit, L"编辑");
    btn(More, L"更多");
    btn(EmptyAction, L"新建第一条提示词");
    add(Body, L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL);
    add(TitleInput, L"EDIT", L"", ES_AUTOHSCROLL);
    add(BodyInput, L"EDIT", L"", ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL);
    add(TagInput, L"EDIT", L"", ES_AUTOHSCROLL);
    SendMessageW(item(TagInput), EM_SETCUEBANNER, TRUE, (LPARAM)L"输入新标签，回车添加");
    btn(TagMenu, L"选择标签");
    btn(Save, L"保存");
    btn(Cancel, L"取消");
    style();
    ready = true;
    reloadTags();
    reload();
    SetFocus(item(Search));
}
void LibraryWindow::style() {
    colors = Palette(readSetting(L"Theme", L"light") == L"dark");
    DeleteObject(backdropBitmap);
    backdropBitmap = nullptr;
    DeleteObject(paperBrush);
    DeleteObject(railBrush);
    paperBrush = CreateSolidBrush(colors.paper);
    railBrush = CreateSolidBrush(colors.rail);
    auto old = fonts;
    HDC dc = GetDC(h);
    hanFace = installedFace(dc, L"Noto Serif SC")         ? L"Noto Serif SC"
              : installedFace(dc, L"Source Han Serif SC") ? L"Source Han Serif SC"
              : installedFace(dc, L"STSong")              ? L"STSong"
                                                          : L"SimSun";
    const wchar_t *latinFace = installedFace(dc, L"Anthropic Serif") ? L"Anthropic Serif" : L"Georgia";
    size_t slot = 0;
    for (auto face : {hanFace, latinFace}) {
        fonts[slot++] = fontAt(face, 14, FW_NORMAL, dpi);
        fonts[slot++] = fontAt(face, 14, FW_MEDIUM, dpi);
        fonts[slot++] = fontAt(face, 27, face == hanFace ? FW_MEDIUM : FW_NORMAL, dpi);
        fonts[slot++] = fontAt(face, 17, face == hanFace ? FW_MEDIUM : FW_NORMAL, dpi);
    }
    for (size_t i = 0; i < fonts.size(); ++i) {
        labelFonts[i] = std::make_unique<Gdiplus::Font>(dc, fonts[i]);
        if (labelFonts[i]->GetLastStatus() != Gdiplus::Ok)
            labelFonts[i].reset();
    }
    ReleaseDC(h, dc);
    for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        auto id = GetDlgCtrlID(c);
        if (id == List || id == Body || id == BodyInput || id == Search || id == TagInput)
            SetWindowTheme(c, colors.dark ? L"DarkMode_Explorer" : nullptr, nullptr);
        size_t role = id == Body || id == BodyInput ? 3 : 0;
        if (id == Body && current)
            role = serifIndex(role, current->body);
        SendMessageW(c, WM_SETFONT, (WPARAM)fonts[role], FALSE);
    }
    for (auto f : old)
        DeleteObject(f);
    SendMessageW(item(List), LB_SETITEMHEIGHT, 0, px(48));
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(h, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DWM_SYSTEMBACKDROP_TYPE material = colors.contrast ? DWMSBT_NONE : DWMSBT_TRANSIENTWINDOW;
    DwmSetWindowAttribute(h, DWMWA_SYSTEMBACKDROP_TYPE, &material, sizeof(material));
    BOOL darkFrame = colors.dark;
    DwmSetWindowAttribute(h, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkFrame, sizeof(darkFrame));
    COLORREF border = colors.dark ? colors.rim : DWMWA_COLOR_DEFAULT;
    DwmSetWindowAttribute(h, DWMWA_BORDER_COLOR, &border, sizeof(border));
    COLORREF caption = colors.contrast ? DWMWA_COLOR_DEFAULT : colors.bar;
    DwmSetWindowAttribute(h, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
}
void LibraryWindow::layout() {
    RECT client;
    GetClientRect(h, &client);
    width = MulDiv(client.right, 96, dpi);
    height = MulDiv(client.bottom, 96, dpi);
    split = std::clamp(width * 33 / 100, 240, 350);
    int x = split + 36, rw = width - x - 43;
    auto move = [&](int id, int a, int b, int c, int d, bool visible) {
        HWND ctrl = item(id);
        if (visible) {
            SetWindowPos(ctrl, nullptr, px(a), px(b), std::max(1, px(c)), std::max(1, px(d)),
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        } else
            ShowWindow(ctrl, SW_HIDE);
    };
    move(Search, 55, 94, split - 95, 22, true);
    int tx = 28;
    auto chip = [&](int id, const std::wstring &name, int cw) {
        SetWindowTextW(item(id), name.c_str());
        move(id, tx, 135, cw, 28, true);
        tx += cw + 5;
    };
    chip(FilterAll, L"全部", 48);
    visibleTags = 0;
    HDC dc = GetDC(h);
    auto old = SelectObject(dc, fonts[0]);
    for (int i = 0; i < 4; i++) {
        if (i < (int)tags.size()) {
            SIZE size;
            SelectObject(dc, fonts[serifIndex(0, tags[i].name)]);
            GetTextExtentPoint32W(dc, tags[i].name.c_str(), tags[i].name.size(), &size);
            int cw = std::clamp(MulDiv(size.cx, 96, dpi) + 20, 42, 104);
            bool more = i + 1 < (int)tags.size();
            if (visibleTags == i && tx + cw + (more ? 39 : 0) <= split - 22) {
                chip(FilterFirst + i, tags[i].name, cw);
                ++visibleTags;
                continue;
            }
        }
        ShowWindow(item(FilterFirst + i), SW_HIDE);
    }
    SelectObject(dc, old);
    ReleaseDC(h, dc);
    SetWindowTextW(item(FilterMore), L"···");
    move(FilterMore, tx, 135, 33, 28, visibleTags < (int)tags.size());
    move(List, 24, 211, split - 48, height - 243, true);
    move(CloseWindowButton, 17, 11, 24, 32, true);
    move(MinimizeWindowButton, 41, 11, 24, 32, true);
    move(ZoomWindowButton, 65, 11, 24, 32, true);
    SetWindowTextW(item(ZoomWindowButton), IsZoomed(h) ? L"还原" : L"最大化");
    move(New, width - 119, 11, 87, 32, !editing);
    move(Edit, width - 203, 11, 72, 32, !editing && current.has_value());
    move(More, width - (editing || current ? 263 : 179), 11, 52, 32, true);
    move(Cancel, width - 199, 11, 73, 32, editing);
    move(Save, width - 116, 11, 83, 32, editing);
    move(Body, x, 196, rw, height - 243, !editing && current.has_value());
    move(EmptyAction, x + (rw - 168) / 2, (height + 70) / 2 + 34, 168, 36,
         !editing && !current.has_value());
    move(TitleInput, x + 11, 132, rw - 18, 25, editing);
    move(TagInput, x + 11, 207, rw - 126, 24, editing);
    move(TagMenu, width - 149, 202, 104, 33, editing);
    move(BodyInput, x + 11, 311, rw - 22, height - 365, editing);
    for (int id : std::initializer_list<int>{Search, List, FilterAll, FilterMore, FilterFirst,
                                             FilterFirst + 1, FilterFirst + 2, FilterFirst + 3})
        EnableWindow(item(id), !editing);
    InvalidateRect(h, nullptr, FALSE);
}
HWND openLibrary(HWND) {
    auto path = dataDir();
    std::filesystem::create_directories(path);
    active = std::make_unique<LibraryWindow>(path / L"library.sqlite3");
    WNDCLASSW wc{};
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(100));
    wc.lpszClassName = className;
    RegisterClassW(&wc);
    POINT cursor;
    GetCursorPos(&cursor);
    auto m = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(m, &mi);
    auto r = mi.rcWork;
    UINT dpi = 96, dy = 96;
    GetDpiForMonitor(m, MDT_EFFECTIVE_DPI, &dpi, &dy);
    int w = std::min<int>(r.right - r.left, MulDiv(980, dpi, 96)),
        ht = std::min<int>(r.bottom - r.top, MulDiv(650, dpi, 96));
    auto h = CreateWindowExW(WS_EX_CONTROLPARENT, className, L"提示词库",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, r.left + (r.right - r.left - w) / 2,
                             r.top + (r.bottom - r.top - ht) / 2, w, ht, nullptr, nullptr, wc.hInstance,
                             active.get());
    if (!h) {
        active.reset();
        throw std::runtime_error("无法打开管理窗口。");
    }
    return h;
}
bool libraryKeys(HWND h, MSG &m) { return active && active->h == h && active->keys(m); }
void releaseLibrary() { active.reset(); }
