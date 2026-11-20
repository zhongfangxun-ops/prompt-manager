"""Exercise the packaged app using real Win32 controls on a private desktop."""
from preserve_clipboard import preserve
import pathlib,sqlite3,sys,tempfile,time
from desktop_probe import *
u.OpenClipboard.argtypes=[wt.HWND];u.GetClipboardData.restype=wt.HANDLE
kernel=c.windll.kernel32;kernel.GlobalLock.argtypes=[wt.HANDLE];kernel.GlobalLock.restype=c.c_void_p
kernel.GlobalUnlock.argtypes=[wt.HANDLE]
def text(h,s):
    buf=c.create_unicode_buffer(s);u.SendMessageW(h,12,0,c.addressof(buf))
def read(h):
    size=u.SendMessageW(h,14,0,0);buf=c.create_unicode_buffer(size+1)
    u.SendMessageW(h,13,size+1,c.addressof(buf));return buf.value
def clipboard():
    assert u.OpenClipboard(None)
    try:
        obj=u.GetClipboardData(13)
        ptr=kernel.GlobalLock(obj)
        try:return c.wstring_at(ptr)
        finally:kernel.GlobalUnlock(obj)
    finally:u.CloseClipboard()
def automation(exe,case):
    with tempfile.TemporaryDirectory() as profile:
        p=launch(exe,profile);db=None
        try:
            shell=until(lambda:byprefix(p.pid,"PromptManager.Shell"))
            def open_view():
                u.SendMessageW(shell,OPEN,0,0);return until(lambda:byprefix(p.pid,"PromptManager.Library"))
            w=open_view()
            ctrl=lambda id:u.GetDlgItem(w,id)
            cmd=lambda id:u.SendMessageW(w,273,id,0)
            def add(title,body,tag=""):
                cmd(103);text(ctrl(110),title);text(ctrl(111),body);text(ctrl(112),tag);cmd(114)
            body="甲😀\r\n\t保留术语  \r\n\r\n";add("课程录音",body,"学习")
            add("修改","其他正文","写作")
            db=sqlite3.connect(pathlib.Path(profile)/"library.sqlite3",isolation_level=None)
            if case=="save":
                cmd(104);text(ctrl(110),"编辑中的草稿");db.execute("BEGIN IMMEDIATE")
                u.PostMessageW(w,16,0,0);dialog=until(lambda:byprefix(p.pid,"#32770"))
                until(lambda:u.IsWindowVisible(dialog));u.PostMessageW(dialog,0x400+102,6,0)
                def error_button():
                    h=byprefix(p.pid,"#32770")
                    if h and u.GetDlgItem(h,65535):return u.GetDlgItem(h,1) or u.GetDlgItem(h,2)
                    return 0
                btn=until(error_button);err=u.GetParent(btn);u.PostMessageW(btn,0xF5,0,0)
                until(lambda:not u.IsWindow(err));time.sleep(.12)
                assert u.IsWindow(w) and read(ctrl(110))=="编辑中的草稿","failed save destroyed draft"
                db.execute("ROLLBACK");cmd(114);assert db.execute("select count(*) from prompts where title='编辑中的草稿'").fetchone()[0]==1
            elif case=="copy":
                import win32gui
                own=win32gui.CreateWindow("STATIC","Clipboard test",0,0,0,1,1,0,0,0,None)
                with preserve(own):
                    lst=ctrl(102);area=wt.RECT();u.GetClientRect(lst,c.byref(area))
                    rowht=u.SendMessageW(lst,0x1a1,0,0)
                    def mouse(y,msg=0x201):u.SendMessageW(lst,msg,1,((int(y)&65535)<<16)|(area.right-15))
                    assert u.OpenClipboard(None)
                    try:mouse(rowht*1.5);assert db.execute("select count(*) from prompts where last_used_at is not null").fetchone()[0]==0
                    finally:u.CloseClipboard()
                    time.sleep(.6);mouse(rowht*1.5);assert clipboard()==body
                    if os.environ.get("PROMPT_TEST_NOTEPAD"):
                        note=subprocess.Popen(["notepad.exe"])
                        try:
                            top=until(lambda:byprefix(note.pid,"Notepad"));native=until(lambda:u.FindWindowExW(top,0,"Edit",None))
                            u.SendMessageW(native,0x302,0,0);assert read(native)==body
                            u.SendMessageW(native,0xB9,0,0);u.SendMessageW(top,16,0,0);note.wait(5)
                        finally:
                            if note.poll() is None:note.terminate();note.wait()
                    mouse(rowht*1.5,0x203);assert clipboard()==body,"double-click copied a different row"
                    time.sleep(.6);mouse(rowht*3);assert clipboard()==body,"blank row copied text"
            elif case=="ime":
                cmd(103);text(ctrl(110),"草稿");tag=ctrl(112);text(tag,"文字")
                u.SendMessageW(tag,0x10D,0,0);u.PostMessageW(tag,0x100,13,0);time.sleep(.08)
                assert read(tag)=="文字","candidate confirmation cleared tag"
                u.PostMessageW(tag,0x100,27,0);time.sleep(.08);assert u.IsWindow(w) and not byprefix(p.pid,"#32770")
                u.SendMessageW(tag,0x10E,0,0);text(ctrl(111),"已保存");cmd(114)
            elif case=="flow":
                cmd(130);text(ctrl(101),"无匹配");assert u.SendMessageW(ctrl(102),0x18b,0,0)==0 and not read(ctrl(106))
                cmd(109);text(ctrl(101),"保留术语");assert read(ctrl(106))==body
                cmd(104);text(ctrl(112),"写作");cmd(114)
                assert db.execute("select count(*) from prompt_tags join prompts on prompts.id=prompt_id where title='课程录音'").fetchone()[0]==2
                for pos in (130,131):
                    cmd(pos);assert u.SendMessageW(ctrl(102),0x18b,0,0)==1 and read(ctrl(106))==body
                u.SendMessageW(w,16,0,0);w=open_view();assert read(ctrl(101))=="" and u.SendMessageW(ctrl(102),0x18b,0,0)==2
                text(ctrl(101),"保留术语");assert read(ctrl(106))==body
                u.PostMessageW(w,273,116,0)
                confirm=until(lambda:byprefix(p.pid,"#32770"));yes=until(lambda:u.GetDlgItem(confirm,6))
                u.PostMessageW(yes,0xF5,0,0);until(lambda:not u.IsWindow(confirm))
                until(lambda: db.execute("select count(*) from prompts").fetchone()[0]==1)
            u.SendMessageW(shell,EXIT,0,0);assert p.wait(5)==0
        finally:
            if db:db.close()
            if p.poll() is None:p.terminate();p.wait()
if __name__=="__main__":automation(pathlib.Path(sys.argv[1]).resolve(),sys.argv[2])

