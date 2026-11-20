import pathlib,sys,tempfile
from desktop_probe import *
def test(exe):
    with tempfile.TemporaryDirectory() as profile:
        p=launch(exe,profile)
        try:
            shell=until(lambda:byprefix(p.pid,"PromptManager.Shell"))
            u.SendMessageW(shell,OPEN,0,0)
            win=until(lambda:byprefix(p.pid,"PromptManager.Library"))
            assert u.GetDlgItem(win,101) and u.GetDlgItem(win,102)
        finally:
            p.terminate();p.wait()
if __name__=="__main__": test(pathlib.Path(sys.argv[1]).resolve())
