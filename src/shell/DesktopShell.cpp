#include "platform/App.h"
#include <shellapi.h>
#include "shell/EdgeButton.h"
#include <commctrl.h>
namespace {
constexpr UINT trayCallback=WM_APP+5;
HWND manager{};
UINT taskbarCreated{};
NOTIFYICONDATAW tray{};
bool visible=true;
constexpr wchar_t runKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
std::wstring startupName(){return L"PromptManager"+suffix();}
bool startupEnabled(){
    DWORD bytes=0;
    return RegGetValueW(HKEY_CURRENT_USER,runKey,startupName().c_str(),RRF_RT_REG_SZ,nullptr,nullptr,&bytes)==ERROR_SUCCESS;
}
void startup(HWND parent){
    LSTATUS error;
    if(startupEnabled()) error=RegDeleteKeyValueW(HKEY_CURRENT_USER,runKey,startupName().c_str());
    else {
        wchar_t path[32768]; GetModuleFileNameW(nullptr,path,32768);
        auto cmd=L"\""+std::wstring(path)+L"\"";
        error=RegSetKeyValueW(HKEY_CURRENT_USER,runKey,startupName().c_str(),REG_SZ,cmd.c_str(),(cmd.size()+1)*sizeof(wchar_t));
    }
    if(error) MessageBoxW(parent,L"无法更改登录启动项。",L"提示词",MB_ICONERROR);
}
void menu(HWND h){
    auto m=CreatePopupMenu();
    AppendMenuW(m,MF_STRING,1,L"打开提示词");
    AppendMenuW(m,MF_STRING,2,visible?L"收起悬浮入口":L"显示悬浮入口");
    AppendMenuW(m,MF_STRING|(startupEnabled()?MF_CHECKED:0),3,L"登录时启动（遵循系统设置）");
    AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,4,L"退出");
    POINT p;GetCursorPos(&p);SetForegroundWindow(h);
    int choice=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,h,nullptr);
    DestroyMenu(m);PostMessageW(h,WM_NULL,0,0);
    if(choice==1) SendMessageW(h,OpenMessage,0,0);
    if(choice==2) SendMessageW(h,EdgeMessage,0,0);
    if(choice==3) startup(h);
    if(choice==4) SendMessageW(h,ExitMessage,0,0);
}
LRESULT CALLBACK shellProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(taskbarCreated && msg==taskbarCreated){Shell_NotifyIconW(NIM_ADD,&tray);return 0;}
    switch(msg){
    case WM_CREATE:
        taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
        tray.cbSize=sizeof(tray);tray.hWnd=h;tray.uID=1;
        tray.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;tray.uCallbackMessage=trayCallback;
        tray.hIcon=LoadIconW(nullptr,IDI_APPLICATION);lstrcpyW(tray.szTip,L"提示词 · 打开提示词库");
        Shell_NotifyIconW(NIM_ADD,&tray);
        visible=readSetting(L"Visible",L"1")==L"1";createEdge(h,visible);return 0;
    case OpenMessage:
        if(!IsWindow(manager)) {try{manager=openLibrary(h);}catch(const std::exception& e){MessageBoxW(h,wideError(e).c_str(),L"打开提示词库失败",MB_ICONERROR);}}
        if(IsWindow(manager)){ShowWindow(manager,IsIconic(manager)?SW_RESTORE:SW_SHOW);SetForegroundWindow(manager);}return 0;
    case RefreshEdge:refreshEdge(visible);return 0; case EdgeMessage: visible=!visible;writeSetting(L"Visible",visible?L"1":L"0");SendMessageW(h,RefreshEdge,0,0);return 0;
    case trayCallback:
        if(l==WM_LBUTTONUP) SendMessageW(h,OpenMessage,0,0);
        if(l==WM_RBUTTONUP) menu(h);
        return 0;
    case WM_CLOSE:case ExitMessage:
        if(IsWindow(manager)){SendMessageW(manager,WM_CLOSE,0,0);if(IsWindow(manager))return 0;}
        DestroyWindow(h);return 0;
    case WM_DESTROY:closeEdge();Shell_NotifyIconW(NIM_DELETE,&tray);PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,msg,w,l);
}
}
int runDesktop(){
    std::wstring name=L"PromptManager.Shell"+suffix(),lockName=L"Local\\"+name;
    HANDLE lock=CreateMutexW(nullptr,FALSE,lockName.c_str());
    if(!lock) throw std::runtime_error("Cannot create application mutex");
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        for(int i=0;i<80;i++){HWND h=FindWindowW(name.c_str(),nullptr);if(h){DWORD pid;GetWindowThreadProcessId(h,&pid);AllowSetForegroundWindow(pid);PostMessageW(h,OpenMessage,0,0);break;}Sleep(25);}
        CloseHandle(lock);return 0;
    }
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=name.c_str();wc.lpfnWndProc=shellProc;
    RegisterClassW(&wc);auto h=CreateWindowW(name.c_str(),L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    if(!h) throw std::runtime_error("Cannot initialize desktop shell");
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){
        if(IsWindow(manager)&&libraryKeys(manager,msg)) continue;
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }CloseHandle(lock);return 0;
}
