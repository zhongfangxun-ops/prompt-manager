"""Release benchmark. Uses temporary profiles and synthetic content only."""
import argparse, ctypes as c, json, math, os, pathlib, platform, shutil, sqlite3, statistics, sys, tempfile, time
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tests"))
from desktop_probe import u, launch, byprefix, until, OPEN, EXIT
import psutil
def message_text(h, s):
    value=c.create_unicode_buffer(s)
    return u.SendMessageW(h,12,0,c.addressof(value))
def shell_of(p):return until(lambda:byprefix(p.pid,"PromptManager.Shell"))
def open_view(p,shell):
    u.SendMessageW(shell,OPEN,0,0)
    w=until(lambda:byprefix(p.pid,"PromptManager.Library"))
    assert u.IsWindowVisible(w) and u.GetDlgItem(w,101)
    u.UpdateWindow(w)
    return w
def close_view(w):u.SendMessageW(w,16,0,0)
def seed(exe,path):
    p=launch(exe,path);h=shell_of(p);t=time.perf_counter();w=open_view(p,h)
    empty_ms=(time.perf_counter()-t)*1000
    close_view(w);u.SendMessageW(h,EXIT,0,0);p.wait(5)
    db=sqlite3.connect(path/"library.sqlite3")
    labels=["学习","写作","开发","翻译"]+[f"分类{i}" for i in range(16)]
    db.executemany("insert into tags(id,name) values(?,?)",enumerate(labels,1))
    body="整理以下内容，保留术语与段落。 Read the source carefully.\n"
    with db:
        for i in range(1001):
            text=(body*37)[:1024] if i<1000 else ("长文\n"*18000)
            db.execute("insert into prompts(id,title,body,created_at,updated_at) values(?,?,?,?,?)",(i+1,f"笔记整理 {i+1}",text,i,i))
            db.execute("insert into prompt_tags values(?,?)",(i+1,i%20+1))
    db.close();return empty_ms
def p95(values):return round(sorted(values)[math.ceil(.95*len(values))-1],2)
def timing(exe,base,root):
    first=[];reopen=[];search=[]
    for i in range(20):
        path=root/f"time-{i}";path.mkdir();shutil.copy2(base/"library.sqlite3",path/"library.sqlite3")
        p=launch(exe,path)
        try:
            h=shell_of(p);t=time.perf_counter();w=open_view(p,h);first.append((time.perf_counter()-t)*1000);close_view(w)
            t=time.perf_counter();w=open_view(p,h);reopen.append((time.perf_counter()-t)*1000)
            if i==0:
                u.SendMessageW(w,273,130,0)
                for word in ["术语","READ","未命中"]*12:
                    t=time.perf_counter();message_text(u.GetDlgItem(w,101),word)
                    u.UpdateWindow(u.GetDlgItem(w,102));u.UpdateWindow(w);search.append((time.perf_counter()-t)*1000)
            close_view(w);u.SendMessageW(h,EXIT,0,0);p.wait(5)
        finally:
            if p.poll() is None:p.terminate();p.wait()
    return {k:{"n":len(v),"median_ms":round(statistics.median(v),2),"p95_ms":p95(v)} for k,v in (("first",first),("reopen",reopen),("search",search))}
def snapshot(p):
    info=p.memory_info();cpu=p.cpu_times()
    handle=c.windll.kernel32.OpenProcess(0x410,False,p.pid)
    try:return [info.private/1048576,info.rss/1048576,cpu.user+cpu.system,u.GetGuiResources(handle,0),u.GetGuiResources(handle,1)]
    finally:c.windll.kernel32.CloseHandle(handle)
def idle(exe,base,root):
    group=[];reports={}
    try:
        for name in ("background","open","closed","closed30"):
            path=root/name;path.mkdir();shutil.copy2(base/"library.sqlite3",path/"library.sqlite3")
            p=launch(exe,path);group.append((name,p));h=shell_of(p)
            loops=30 if name=="closed30" else (0 if name=="background" else 1)
            trend=[]
            for i in range(loops):
                w=open_view(p,h)
                if name!="open":close_view(w)
                if i in (0,9,19,29):trend.append([i+1,*snapshot(psutil.Process(p.pid))])
            reports[name]={"cycles":trend}
        time.sleep(60)
        series={name:[] for name,p in group}
        for i in range(61):
            for name,p in group:series[name].append(snapshot(psutil.Process(p.pid)))
            if i<60:time.sleep(1)
        for name,s in series.items():
            reports[name].update(private_mib=round(statistics.median(v[0] for v in s),2),working_set_mib=round(statistics.median(v[1] for v in s),2),cpu_s=round(s[-1][2]-s[0][2],4),gdi=s[-1][3],user=s[-1][4])
    finally:
        for _,p in group:
            if p.poll() is None:
                h=byprefix(p.pid,"PromptManager.Shell")
                if h:u.SendMessageW(h,EXIT,0,0)
                p.wait(5)
    return reports
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("exe",type=pathlib.Path);parser.add_argument("output",type=pathlib.Path);args=parser.parse_args()
    exe=args.exe.resolve()
    with tempfile.TemporaryDirectory() as tmp:
        root=pathlib.Path(tmp);base=root/"seed";base.mkdir()
        report={"os":platform.platform(),"dpi":u.GetDpiForSystem(),"mode":"x64 Release; warm OS cache; independent isolated instances","empty_ms":round(seed(exe,base),2)}
        report["timing"]=timing(exe,base,root);print(report,flush=True)
        report["idle"]=idle(exe,base,root);args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8");print("Measured",flush=True)

