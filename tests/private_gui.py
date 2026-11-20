"""Run a Win32 integration check on an undisplayed desktop."""
import os, pathlib, subprocess, sys, uuid
import win32con,win32event,win32process,win32service
def run():
    name="PromptManagerTest"+uuid.uuid4().hex
    desktop=win32service.CreateDesktop(name,0,win32con.GENERIC_ALL,None)
    try:
        info=win32process.STARTUPINFO();info.lpDesktop="winsta0\\"+name
        command=subprocess.list2cmdline([sys.executable,*sys.argv[1:]])
        process,thread,_,_=win32process.CreateProcess(None,command,None,None,False,0,None,None,info)
        try:
            if win32event.WaitForSingleObject(process,40000)!=0:
                win32process.TerminateProcess(process,3);raise TimeoutError(command)
            return win32process.GetExitCodeProcess(process)
        finally:thread.Close();process.Close()
    finally:desktop.CloseDesktop()
if __name__=="__main__":sys.exit(run())

