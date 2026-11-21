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
    FilterFirst = 130
};
struct LibraryWindow {
    HWND h{};
    PromptStore store;
    Palette colors;
    UINT dpi = 96;
    std::array<HFONT, 4> fonts{};
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
    void copy(Id);
    void flash(std::wstring text, Id id = 0);
    void deleteMenu();
    void deleteCurrent();
    bool keys(MSG &);
};
