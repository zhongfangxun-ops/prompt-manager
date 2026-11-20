"""Windows-only probes for the app's window classes, with an isolated database."""
import ctypes as c
from ctypes import wintypes as wt
import os, pathlib, subprocess, sys, tempfile, time
u = c.WinDLL("user32", use_last_error=True)
u.SetProcessDPIAware()
callback = c.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
u.SendMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
u.SendMessageW.restype = wt.LPARAM
u.GetDlgItem.argtypes = [wt.HWND,c.c_int]
u.GetDlgItem.restype = wt.HWND
def windows(pid):
    result = {}
    @callback
    def collect(hwnd, _):
        owner = wt.DWORD()
        u.GetWindowThreadProcessId(hwnd, c.byref(owner))
        if owner.value == pid:
            buf = c.create_unicode_buffer(256)
            u.GetClassNameW(hwnd,buf,256)
            result[buf.value] = hwnd
        return True
    u.EnumWindows(collect,0)
    return result
def until(get, timeout=5):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        value=get()
        if value:return value
        time.sleep(.008)
    raise RuntimeError("Window or condition timed out: "+str(get))
def byprefix(pid,prefix):
    return next((v for k,v in windows(pid).items() if k.startswith(prefix)),0)
def launch(exe, profile):
    return subprocess.Popen([str(exe)],env={**os.environ,"PROMPT_MANAGER_PROFILE":str(profile)})
OPEN,EXIT,EDGE=0x8015,0x8016,0x8017
def smoke(exe):
    with tempfile.TemporaryDirectory() as profile:
        p=launch(exe,profile)
        try:
            shell=until(lambda:byprefix(p.pid,"PromptManager.Shell"))
            second=launch(exe,profile)
            assert second.wait(timeout=4)==0
            win=until(lambda:next((h for k,h in windows(p.pid).items() if k in ("Static","PromptManager.Library")),0))
            u.SendMessageW(win,0x10,0,0)
            assert not u.IsWindow(win) and p.poll() is None
            u.SendMessageW(shell,EXIT,0,0)
            assert p.wait(timeout=4)==0
        finally:
            if p.poll() is None:p.terminate();p.wait()
if __name__=="__main__":smoke(pathlib.Path(sys.argv[1]).resolve())
