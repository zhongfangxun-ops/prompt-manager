"""Exercise Explorer import, repeat import and persistent themes with an isolated profile."""
import ctypes
from contextlib import closing
import json
import pathlib
import sqlite3
import sys
import tempfile

import win32gui
import win32ui

from desktop_probe import EXIT, OPEN, byprefix, launch, u, until


def edit_background(edit):
    # Sample the empty right edge of the search field on the native control itself.
    window = win32gui.GetParent(edit)
    x, y, right, bottom = win32gui.GetWindowRect(window)
    field = win32gui.GetWindowRect(edit)
    dc = win32gui.GetWindowDC(window)
    source = win32ui.CreateDCFromHandle(dc)
    mem = source.CreateCompatibleDC()
    bitmap = win32ui.CreateBitmap()
    bitmap.CreateCompatibleBitmap(source, right - x, bottom - y)
    old = mem.SelectObject(bitmap)
    try:
        assert u.PrintWindow(window, mem.GetSafeHdc(), 2)
        return win32gui.GetPixel(mem.GetSafeHdc(), field[2] - x - 4, (field[1] + field[3]) // 2 - y)
    finally:
        mem.SelectObject(old)
        win32gui.DeleteObject(bitmap.GetHandle())
        mem.DeleteDC()
        source.DeleteDC()
        win32gui.ReleaseDC(window, dc)


def select_file(window, pid, path):
    u.PostMessageW(window, 273, 121, 0)
    dialog = until(lambda: byprefix(pid, "#32770"))
    until(lambda: u.IsWindowVisible(dialog) and win32gui.GetWindowText(dialog) == "导入 Gemini Voyager 提示词")

    def filename_field():
        container = u.GetDlgItem(dialog, 1148)  # cmb13 on Explorer-style open dialogs
        fields = []
        if container:
            win32gui.EnumChildWindows(container, lambda child, _: fields.append(child), None)
        return next((child for child in fields if win32gui.GetClassName(child) == "Edit"), 0) or u.GetDlgItem(dialog, 1152)

    field = until(filename_field)
    buf = ctypes.create_unicode_buffer(str(path))
    u.SendMessageW(field, 12, 0, ctypes.addressof(buf))
    u.PostMessageW(u.GetDlgItem(dialog, 1), 0xF5, 0, 0)
    until(lambda: not u.IsWindow(dialog))


def run(exe, supplied=None):
    with tempfile.TemporaryDirectory() as temp:
        profile = pathlib.Path(temp)
        path = supplied or profile / "示例提示词.json"
        if not supplied:
            path.write_text(json.dumps({
                "format": "gemini-voyager.prompts.v1", "version": "1.0.0",
                "items": [{"id": "example-1", "name": "练习", "text": "第一行\n\n第二行 😀",
                           "tags": ["一", "二", "三", "四", "五", "Mind & Notes"], "createdAt": 1000, "updatedAt": 1000}]
            }, ensure_ascii=False), encoding="utf-8")
        entries = json.loads(path.read_text(encoding="utf-8-sig"))["items"]
        p = launch(exe, profile)
        try:
            def open_library():
                shell = until(lambda: byprefix(p.pid, "PromptManager.Shell"))
                u.SendMessageW(shell, OPEN, 0, 0)
                return shell, until(lambda: byprefix(p.pid, "PromptManager.Library"))

            shell, window = open_library()
            search = u.GetDlgItem(window, 101)
            light = edit_background(search)
            assert u.IsWindowVisible(u.GetDlgItem(window, 105)), "More hidden in empty library"
            u.PostMessageW(u.GetDlgItem(window, 105), 0xF5, 0, 0)
            popup = until(lambda: byprefix(p.pid, "#32768"))
            handle = until(lambda: u.SendMessageW(popup, 0x01E1, 0, 0))  # MN_GETHMENU
            menu_ids = {u.GetMenuItemID(handle, i) for i in range(u.GetMenuItemCount(handle))}
            assert {121, 122, 123} <= menu_ids
            u.PostMessageW(window, 0x102, ord("深"), 0)  # keyboard selection inside TrackPopupMenu
            until(lambda: not u.IsWindow(popup))
            until(lambda: (edit_background(search) & 255) < (light & 255) - 80)
            dark = edit_background(search)
            assert (dark & 255) < (light & 255) - 80, (light, dark)

            with closing(sqlite3.connect(profile / "library.sqlite3")) as db:
                for _ in range(2):
                    select_file(window, p.pid, path.resolve())
                    def imported():
                        error = byprefix(p.pid, "#32770")
                        if error and win32gui.GetWindowText(error) == "导入提示词失败":
                            raise AssertionError(win32gui.GetWindowText(u.GetDlgItem(error, 65535)))
                        return db.execute("select count(*) from prompts").fetchone()[0] == len(entries)
                    until(imported)
                    # Wait for completion even on the second import, whose row count stays constant.
                    u.SendMessageW(window, 0, 0, 0)
                for entry in entries:
                    row = db.execute("select id,body from prompts where title=? and body=?",
                                     (entry["name"], entry["text"])).fetchone()
                    assert row and row[1] == entry["text"]
                    tags = {r[0] for r in db.execute(
                        "select name from tags join prompt_tags on tag_id=tags.id where prompt_id=?", (row[0],))}
                    assert tags == set(entry["tags"])

            if not supplied:
                overflow = u.GetDlgItem(window, 108)
                assert u.IsWindowVisible(overflow)
                u.PostMessageW(overflow, 0xF5, 0, 0)
                popup = until(lambda: byprefix(p.pid, "#32768"))
                until(lambda: u.IsWindowVisible(popup))
                u.PostMessageW(window, 0x102, ord("M"), 0)
                until(lambda: not u.IsWindow(popup))
                u.PostMessageW(overflow, 0xF5, 0, 0)
                popup = until(lambda: byprefix(p.pid, "#32768"))
                menu = until(lambda: u.SendMessageW(popup, 0x01E1, 0, 0))
                assert u.GetMenuState(menu, 0, 0x400) & 8  # first (English) tag is checked
                u.PostMessageW(window, 0x1F, 0, 0)
                until(lambda: not u.IsWindow(popup))
            u.SendMessageW(shell, EXIT, 0, 0)
            assert p.wait(5) == 0
            p = launch(exe, profile)
            shell, window = open_library()
            assert edit_background(u.GetDlgItem(window, 101)) == dark
            u.SendMessageW(window, 273, 103, 0)
            title = u.GetDlgItem(window, 110)
            buf = ctypes.create_unicode_buffer("编辑中的主题切换")
            u.SendMessageW(title, 12, 0, ctypes.addressof(buf))
            u.SendMessageW(window, 273, 122, 0)
            actual = ctypes.create_unicode_buffer(len(buf))
            u.SendMessageW(title, 13, len(actual), ctypes.addressof(actual))
            assert actual.value == buf.value
            assert edit_background(title) == light
            u.SendMessageW(title, 12, 0, 0)
            u.SendMessageW(shell, EXIT, 0, 0)
            assert p.wait(5) == 0
            print(f"{len(entries)} prompts, exact bodies and tags, repeat import, dark/light and restart verified.")
        finally:
            if p.poll() is None:
                p.terminate()
                p.wait()


if __name__ == "__main__":
    run(pathlib.Path(sys.argv[1]).resolve(), pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None)
