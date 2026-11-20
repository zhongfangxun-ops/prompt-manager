"""Exercise foreground and geometry events on an interactive Windows test desktop."""
import pathlib,sys,tempfile
import win32api,win32con,win32gui
from desktop_probe import *
def test(exe):
    with tempfile.TemporaryDirectory() as path:
        p=launch(exe,path);h=until(lambda:byprefix(p.pid,"PromptManager.Shell"));edge=until(lambda:byprefix(p.pid,"PromptManager.Edge"))
        suffix=next(k for k in windows(p.pid) if k.startswith("PromptManager.Shell")).removeprefix("PromptManager.Shell")
        trial=win32gui.CreateWindow("STATIC","Prompt Manager fullscreen test",win32con.WS_POPUP,40,40,150,110,0,0,0,None)
        try:
            info=win32api.GetMonitorInfo(win32api.MonitorFromWindow(edge));left,top,right,bottom=info['Monitor']
            win32gui.ShowWindow(trial,win32con.SW_SHOW)
            win32gui.SetForegroundWindow(trial);until(lambda:u.GetForegroundWindow()==trial)
            win32gui.MoveWindow(trial,left,top,right-left,bottom-top,True);until(lambda:not u.IsWindowVisible(edge))
            win32gui.MoveWindow(trial,left+20,top+20,150,110,True);until(lambda:u.IsWindowVisible(edge))
            u.SendMessageW(h,EDGE,0,0);win32gui.MoveWindow(trial,left,top,right-left,bottom-top,True)
            win32gui.MoveWindow(trial,left+20,top+20,150,110,True);time.sleep(.12);assert not u.IsWindowVisible(edge)
            u.SendMessageW(h,EDGE,0,0)
        finally:
            win32gui.DestroyWindow(trial);u.SendMessageW(h,EXIT,0,0);p.wait(5)
            import winreg
            if suffix:
                try:winreg.DeleteKey(winreg.HKEY_CURRENT_USER,"Software\\PromptManager"+suffix)
                except FileNotFoundError:pass
if __name__=="__main__":test(pathlib.Path(sys.argv[1]).resolve())
