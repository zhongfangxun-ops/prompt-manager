#include "platform/App.h"
#include "shell/Placement.h"
#include "shell/EdgeButton.h"
#include <objidl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <cmath>
namespace {
HWND button{},shell{}; HMONITOR monitor{}; Anchor anchor{};
HWINEVENTHOOK foregroundHook{},locationHook{}; DWORD trackedThread{};
ULONG_PTR graphics{}; bool wanted=true,hot=false,down=false,drag=false;
POINT press{},offset{}; int width=36,height=52;
MONITORINFOEXW info(HMONITOR m){
    MONITORINFOEXW v{};v.cbSize=sizeof(v);GetMonitorInfoW(m,&v);return v;
}
bool full(){
    HWND f=GetForegroundWindow();if(!f || IsIconic(f) || f==GetDesktopWindow() || f==GetShellWindow())return false;
    DWORD pid;GetWindowThreadProcessId(f,&pid);if(pid==GetCurrentProcessId())return false;
    wchar_t cls[128]{};GetClassNameW(f,cls,128);
    if(!wcscmp(cls,L"WorkerW")||!wcscmp(cls,L"Shell_TrayWnd")||!wcscmp(cls,L"Progman"))return false;
    if(MonitorFromWindow(f,MONITOR_DEFAULTTONEAREST)!=monitor)return false;
    if(IsZoomed(f)&&(GetWindowLongPtrW(f,GWL_STYLE)&WS_CAPTION))return false;
    RECT r{};if(FAILED(DwmGetWindowAttribute(f,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r))))GetWindowRect(f,&r);
    RECT m=info(monitor).rcMonitor;
    return r.left<=m.left&&r.right>=m.right&&r.top<=m.top&&r.bottom>=m.bottom;
}
void visibility(){
    bool display=shouldShow(wanted,full());
    if(bool(IsWindowVisible(button))!=display)ShowWindow(button,display?SW_SHOWNOACTIVATE:SW_HIDE);
}
void CALLBACK event(HWINEVENTHOOK,DWORD type,HWND h,LONG object,LONG child,DWORD,DWORD){
    if(type==EVENT_SYSTEM_FOREGROUND || (object==OBJID_WINDOW && child==0 && h==GetForegroundWindow()))
        PostMessageW(shell,RefreshEdge,0,0);
}
void watch(){
    DWORD pid=0,tid=GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    if(tid==trackedThread)return;
    if(locationHook)UnhookWinEvent(locationHook);
    locationHook=nullptr;trackedThread=tid;
    if(tid&&pid!=GetCurrentProcessId())
        locationHook=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,event,pid,tid,WINEVENT_OUTOFCONTEXT);
}
void paint(){
    if(!button)return;
    using namespace Gdiplus;
    HDC screen=GetDC(nullptr),mem=CreateCompatibleDC(screen);BITMAPINFO bi{};
    bi.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB,0,0,0,0,0};
    void* bits{};auto bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap || !bits){DeleteDC(mem);ReleaseDC(nullptr,screen);return;}
    auto old=SelectObject(mem,bitmap);
    {
        Bitmap surface(width,height,width*4,PixelFormat32bppPARGB,(BYTE*)bits);
        Graphics g(&surface);g.Clear(Color(0,0,0,0));g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.ScaleTransform(width/36.f,height/52.f);
        GraphicsPath path;path.AddArc(2,2,32,32,180,180);path.AddArc(2,18,32,32,0,180);path.CloseFigure();
        HIGHCONTRASTW hc{sizeof(hc)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);
        bool contrast=hc.dwFlags&HCF_HIGHCONTRASTON;
        LinearGradientBrush glass(Point(1,2),Point(34,50),contrast?Color(255,0,0,0):Color(hot?251:231,138,159,176),contrast?Color(255,0,0,0):Color(232,33,52,73));
        g.FillPath(&glass,&path);Pen rim(Color(220,245,252,255),1.25f);g.DrawPath(&rim,&path);
        Pen ink(Color(255,255,255,255),1.9f);ink.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
        g.DrawLine(&ink,11,21,15,25);g.DrawLine(&ink,15,25,11,29);g.DrawLine(&ink,19,29,25,29);
    }
    POINT origin{};SIZE sz{width,height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    UpdateLayeredWindow(button,screen,nullptr,&sz,mem,&origin,0,&blend,ULW_ALPHA);
    SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(nullptr,screen);
}
void place(){
    MONITORINFOEXW m=info(monitor);if(!m.szDevice[0]){monitor=MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY);m=info(monitor);}
    UINT dpi=96,dy=96;GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dpi,&dy);
    width=MulDiv(36,dpi,96);height=MulDiv(52,dpi,96);
    auto p=resolveAnchor(m.rcWork,anchor,width,height);
    SetWindowPos(button,HWND_TOPMOST,p.x,p.y,width,height,SWP_NOACTIVATE);
    paint();visibility();
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:{
        GetCursorPos(&press);RECT r;GetWindowRect(h,&r);offset={press.x-r.left,press.y-r.top};down=true;drag=false;SetCapture(h);return 0;}
    case WM_MOUSEMOVE:{
        if(!hot){hot=true;TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);paint();}
        if(down){POINT p;GetCursorPos(&p);
            if(abs(p.x-press.x)>GetSystemMetrics(SM_CXDRAG)||abs(p.y-press.y)>GetSystemMetrics(SM_CYDRAG))drag=true;
            if(drag){monitor=MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST);anchor=nearestAnchor(info(monitor).rcWork,{p.x-offset.x,p.y-offset.y},width,height);place();}}
        return 0;}
    case WM_LBUTTONUP:
        if(down){down=false;ReleaseCapture();
            if(drag){writeSetting(L"Monitor",info(monitor).szDevice);writeSetting(L"Side",std::to_wstring(anchor.side));writeSetting(L"Fraction",std::to_wstring(anchor.fraction));}
            else PostMessageW(shell,OpenMessage,0,0);}
        return 0;
    case WM_CAPTURECHANGED:down=false;return 0;
    case WM_MOUSELEAVE:hot=false;paint();return 0;
    case WM_DISPLAYCHANGE:case WM_SETTINGCHANGE:case WM_THEMECHANGED:if(button)place();return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
}
void createEdge(HWND owner,bool visible){
    shell=owner;wanted=visible;Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&graphics,&input,nullptr);
    monitor=MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY);auto device=readSetting(L"Monitor");
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM p)->BOOL{
        if(*reinterpret_cast<std::wstring*>(p)==info(m).szDevice){monitor=m;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&device));
    anchor={std::clamp(_wtoi(readSetting(L"Side",L"1").c_str()),0,3),_wtoi(readSetting(L"Fraction",L"5000").c_str())};
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName=L"PromptManager.Edge";wc.hCursor=LoadCursorW(nullptr,IDC_HAND);
    RegisterClassW(&wc);button=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TOPMOST,wc.lpszClassName,L"打开提示词",WS_POPUP,0,0,36,52,nullptr,nullptr,wc.hInstance,nullptr);
    foregroundHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,event,0,0,WINEVENT_OUTOFCONTEXT);
    place();watch();
}
void refreshEdge(bool visible){wanted=visible;watch();visibility();}
void closeEdge(){
    if(foregroundHook)UnhookWinEvent(foregroundHook);
    if(locationHook)UnhookWinEvent(locationHook);
    DestroyWindow(button);button=nullptr;if(graphics)Gdiplus::GdiplusShutdown(graphics);
}
