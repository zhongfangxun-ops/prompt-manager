"""Preserve byte-backed clipboard formats without decoding or logging their content."""
import ctypes as c
from ctypes import wintypes as wt
from contextlib import contextmanager
u=c.windll.user32;k=c.windll.kernel32
k.GlobalSize.argtypes=[wt.HGLOBAL];k.GlobalSize.restype=c.c_size_t
k.GlobalLock.argtypes=[wt.HGLOBAL];k.GlobalLock.restype=c.c_void_p
k.GlobalUnlock.argtypes=[wt.HGLOBAL]
k.GlobalAlloc.argtypes=[wt.UINT,c.c_size_t];k.GlobalAlloc.restype=wt.HGLOBAL
u.GetClipboardData.restype=wt.HANDLE
u.SetClipboardData.argtypes=[wt.UINT,wt.HANDLE];u.SetClipboardData.restype=wt.HANDLE
@contextmanager
def preserve(owner):
    if not u.OpenClipboard(owner):raise RuntimeError("Clipboard busy")
    data=[];fmt=0
    try:
        while fmt:=u.EnumClipboardFormats(fmt):
            if fmt in (2,3,9,14,0x80,0x82,0x83,0x8E) or 0x300<=fmt<=0x3ff:
                raise RuntimeError("Clipboard contains non-byte formats; run this test with a clean clipboard.")
            h=u.GetClipboardData(fmt);n=k.GlobalSize(h);ptr=k.GlobalLock(h)
            if not ptr:raise RuntimeError("Clipboard cannot be preserved")
            try:data.append((fmt,c.string_at(ptr,n)))
            finally:k.GlobalUnlock(h)
    finally:u.CloseClipboard()
    try:yield
    finally:
        if not u.OpenClipboard(owner):raise RuntimeError("Could not restore clipboard")
        try:
            u.EmptyClipboard()
            for fmt,raw in data:
                h=k.GlobalAlloc(2,len(raw));ptr=k.GlobalLock(h)
                c.memmove(ptr,raw,len(raw));k.GlobalUnlock(h)
                if not u.SetClipboardData(fmt,h):raise RuntimeError("Could not restore clipboard format")
        finally:u.CloseClipboard()

