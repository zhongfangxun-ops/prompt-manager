"""Render native application windows with sample content into local PNG previews."""
import ctypes as c, pathlib, sys, tempfile, sqlite3, time
from ctypes import wintypes as wt
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tests"))
from desktop_probe import *
from PIL import Image
import win32gui,win32ui
def capture(hwnd,path):
    x,y,r,b=win32gui.GetWindowRect(hwnd);w,h=r-x,b-y
    hdc=win32gui.GetWindowDC(hwnd);source=win32ui.CreateDCFromHandle(hdc);mem=source.CreateCompatibleDC()
    bmp=win32ui.CreateBitmap();bmp.CreateCompatibleBitmap(source,w,h);old=mem.SelectObject(bmp)
    try:
        if not u.PrintWindow(hwnd,mem.GetSafeHdc(),2):raise RuntimeError("PrintWindow failed")
        im=Image.frombytes("RGB",(w,h),bmp.GetBitmapBits(True),"raw","BGRX");im.save(path)
    finally:
        mem.SelectObject(old);win32gui.DeleteObject(bmp.GetHandle());mem.DeleteDC();source.DeleteDC();win32gui.ReleaseDC(hwnd,hdc)
def main(exe,out):
    with tempfile.TemporaryDirectory() as temp:
        p=launch(exe,temp)
        try:
            h=until(lambda:byprefix(p.pid,"PromptManager.Shell"));u.SendMessageW(h,OPEN,0,0)
            win=until(lambda:byprefix(p.pid,"PromptManager.Library"));u.SendMessageW(win,16,0,0)
            with sqlite3.connect(pathlib.Path(temp)/"library.sqlite3") as db:
                tags=["学习","写作","编程"];db.executemany("insert into tags(id,name) values(?,?)",enumerate(tags,1))
                examples=[("课程笔记整理","请将下面的课程记录整理成一份便于复习的笔记。\r\n\r\n整理要求\r\n\r\n1. 按课堂顺序划分主题，保留重要术语。\r\n2. 每个概念用一句话解释，再给一个例子。\r\n3. 把仍有疑问的地方单独列出。\r\n\r\n课程记录\r\n[在这里粘贴原文]",[1,2]),("中文表达润色","保留原文结构，精简重复表达。",[2]),("代码审阅","检查边界条件，并给出能够复现问题的测试。",[3]),("论文阅读提纲","按问题、方法和证据三个部分整理。",[1]),("每周学习复盘","从本周笔记中提取三个值得回顾的问题。",[1,2])]
                for i,(name,body,tags) in enumerate(examples,1):
                    db.execute("insert into prompts values(?,?,?,?,?,?)",(i,name,body,10-i,10-i,None))
                    db.executemany("insert into prompt_tags values(?,?)",[(i,t) for t in tags])
            db.close()
            u.SendMessageW(h,OPEN,0,0);win=until(lambda:byprefix(p.pid,"PromptManager.Library"));u.UpdateWindow(win)
            out.mkdir(parents=True,exist_ok=True);capture(win,out/"library.png")
            u.SendMessageW(win,273,104,0);u.UpdateWindow(win);capture(win,out/"editor.png")
            u.SendMessageW(win,273,116,0);u.SendMessageW(h,EXIT,0,0);p.wait(5)
        finally:
            if p.poll() is None:p.terminate();p.wait()
if __name__=="__main__":main(pathlib.Path(sys.argv[1]).resolve(),pathlib.Path(sys.argv[2]))
