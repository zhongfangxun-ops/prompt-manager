#pragma once
#include "platform/App.h"
#include "ui/Style.h"
#include "ui/RowCopy.h"
#include "ui/PromptEditor.h"
#include <commctrl.h>
#include <array>
enum Control {
    Search = 101,
    List,
    New,
    Edit,
    More,
    Body,
    FilterAll,
    FilterMore,
    EmptyAction,
    TitleInput,
    BodyInput,
    TagInput,
    TagMenu,
    Save,
    Cancel,
    DeleteEntry,
    CopyEntry,
    CloseWindowButton,
    MinimizeWindowButton,
    ZoomWindowButton,
    ImportVoyager,
    LightTheme,
    DarkTheme,
    FilterFirst = 130
};
struct LibraryWindow {
    HWND h{};
    PromptStore store;
    Palette colors;
    UINT dpi = 96;
    const wchar_t *hanFace = L"SimSun";
    std::array<HFONT, 8> fonts{};
    std::array<std::unique_ptr<Gdiplus::Font>, 8> labelFonts;
    void label(HDC dc, std::wstring_view text, RECT bounds, size_t fontIndex, COLORREF color,
               UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS) const;
    HBITMAP backdropBitmap{};
    SIZE backdropSize{};
    void paintBackdrop(HDC dc, RECT bounds);
    HBRUSH paperBrush{}, railBrush{};
    std::vector<Tag> tags;
    std::vector<Summary> rows;
    std::optional<Prompt> current;
    Id category{}, copied{};
    std::wstring notice;
    RowCopyGesture rowGesture;
    HWND composing{};
    EditorState editor;
    bool editing = false, ready = false, loading = false;
    bool keyboardFocus = false;
    BOOL animations = TRUE;
    HWND motionButton{};
    bool motionPressed{};
    ULONGLONG motionStart{};
    float motionFrom{};
    float motionDepth(ULONGLONG now) const;
    float pressDepth(HWND button, bool pressed);
    void advancePress();
    int width = 900, height = 580, split = 302, visibleTags = 0;
    explicit LibraryWindow(const std::filesystem::path &path) : store(path) {}
    ~LibraryWindow();
    HWND item(int id) const { return GetDlgItem(h, id); }
    int px(int n) const { return MulDiv(n, dpi, 96); }
    RECT rect(int x, int y, int w, int ht) const { return {px(x), px(y), px(x + w), px(y + ht)}; }
    void create();
    void style();
    void layout();
    void paint();
    bool draw(const DRAWITEMSTRUCT &);
    void showDetail();
    void reload(Id preferred = 0);
    void reloadTags();
    void pick(int index);
    void command(int id, int code);
    void edit(bool fresh);
    bool save();
    bool leaveEdit();
    void cancelEdit();
    void chooseTag();
    void filter(Id);
    void filterMenu();
    HMENU activePopup{};
    HWND styledPopup{};
    int trackPopup(int control, HMENU menu);
    bool measurePopup(MEASUREITEMSTRUCT &item);
    bool drawPopup(const DRAWITEMSTRUCT &item);
    LRESULT popupChar(wchar_t key);
    void stylePopup(HWND popup);
    void copy(Id);
    void flash(std::wstring text, Id id = 0);
    void moreMenu();
    void importFile();
    void deleteCurrent();
    bool keys(MSG &);
};
