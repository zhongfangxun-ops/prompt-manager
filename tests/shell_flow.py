import pathlib,sys,tempfile,winreg
from desktop_probe import *
def test(exe):
    with tempfile.TemporaryDirectory() as path:
        p=launch(exe,path);shell=until(lambda:byprefix(p.pid,"PromptManager.Shell"))
        name=next(k for k in windows(p.pid) if k.startswith("PromptManager.Shell"))
        suffix=name.removeprefix("PromptManager.Shell")
        settings="Software\\PromptManager"+suffix
        run=r"Software\Microsoft\Windows\CurrentVersion\Run";entry="PromptManager"+suffix
        try:
            edge=until(lambda:byprefix(p.pid,"PromptManager.Edge"))
            u.SendMessageW(shell,273,2,0);assert not u.IsWindowVisible(edge)
            u.SendMessageW(shell,273,3,0)
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,run) as key:
                assert winreg.QueryValueEx(key,entry)[0]=='"'+str(exe)+'"'
            u.SendMessageW(shell,273,3,0)
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,run) as key:
                try:winreg.QueryValueEx(key,entry)
                except FileNotFoundError:pass
                else:raise AssertionError("startup switch left an entry")
            u.SendMessageW(shell,273,4,0);assert p.wait(4)==0
            p=launch(exe,path);shell=until(lambda:byprefix(p.pid,"PromptManager.Shell"));edge=until(lambda:byprefix(p.pid,"PromptManager.Edge"))
            assert not u.IsWindowVisible(edge) and not (pathlib.Path(path)/"library.sqlite3").exists()
            u.SendMessageW(shell,273,2,0);u.SendMessageW(shell,273,1,0);assert byprefix(p.pid,"PromptManager.Library")
            u.SendMessageW(shell,273,4,0);assert p.wait(4)==0
        finally:
            if p.poll() is None:p.terminate();p.wait()
            try:winreg.DeleteKey(winreg.HKEY_CURRENT_USER,settings)
            except FileNotFoundError:pass
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,run,0,winreg.KEY_SET_VALUE) as key:
                try:winreg.DeleteValue(key,entry)
                except FileNotFoundError:pass
if __name__=="__main__":test(pathlib.Path(sys.argv[1]).resolve())

