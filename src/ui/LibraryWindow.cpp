#include "ui/LibraryWindow.h"
#include "platform/Clipboard.h"
#include <dwmapi.h>
namespace {
constexpr wchar_t className[]=L"PromptManager.Library";
LRESULT CALLBACK rowMouse(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
    auto& view=*reinterpret_cast<LibraryWindow*>(data);
    if(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONDBLCLK){
        RECT r;GetClientRect(h,&r);int x=(short)LOWORD(l);
        if(x>=r.right-view.px(57)){
            LRESULT hit=SendMessageW(h,LB_ITEMFROMPOINT,0,l);
            if(!HIWORD(hit)&&LOWORD(hit)<view.rows.size()){
                try{view.copy(view.rows[LOWORD(hit)].id);}catch(const std::exception& e){view.flash(wideError(e));}
                return 0;
            }
        }
    }
    return DefSubclassProc(h,msg,w,l);
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    auto* v=reinterpret_cast<LibraryWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE){v=static_cast<LibraryWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);v->h=h;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)v);}
    if(!v)return DefWindowProcW(h,msg,w,l);
    try{
        switch(msg){
        case WM_CREATE:v->create();return 0;
        case WM_SIZE:if(v->ready && (v->width!=MulDiv(LOWORD(l),96,v->dpi)||v->height!=MulDiv(HIWORD(l),96,v->dpi)))v->layout();return 0;
        case WM_GETMINMAXINFO:{auto m=reinterpret_cast<MINMAXINFO*>(l);auto mon=MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(mon,&mi);m->ptMinTrackSize={std::min(v->px(650),int(mi.rcWork.right-mi.rcWork.left)),std::min(v->px(460),int(mi.rcWork.bottom-mi.rcWork.top))};return 0;}
        case WM_DPICHANGED:{v->dpi=HIWORD(w);v->style();auto r=(RECT*)l;SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);v->layout();return 0;}
        case WM_SETTINGCHANGE:case WM_THEMECHANGED:if(v->ready){v->style();v->layout();}break;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:v->paint();return 0;
        case WM_DRAWITEM:if(v->draw(*(DRAWITEMSTRUCT*)l))return TRUE;break;
        case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{
            auto dc=(HDC)w;auto id=GetDlgCtrlID((HWND)l);bool rail=id==List;
            SetTextColor(dc,v->colors.ink);SetBkColor(dc,rail?v->colors.rail:v->colors.paper);
            return (LRESULT)(rail?v->railBrush:v->paperBrush);}
        case WM_COMMAND:if(v->ready&&!v->loading)v->command(LOWORD(w),HIWORD(w));return 0;
        case WM_TIMER:if(w==9){KillTimer(h,9);v->notice.clear();v->copied=0;InvalidateRect(h,nullptr,FALSE);InvalidateRect(v->item(List),nullptr,FALSE);}return 0;
        case WM_SETFOCUS:if(v->ready)SetFocus(v->item(v->editing?TitleInput:Search));return 0;
        case WM_CLOSE:if(!v->editing||v->leaveEdit())DestroyWindow(h);return 0;
        case WM_NCDESTROY:PostThreadMessageW(GetCurrentThreadId(),WM_NULL,0,0);SetWindowLongPtrW(h,GWLP_USERDATA,0);return DefWindowProcW(h,msg,w,l);
        }
    }catch(const std::exception& e){MessageBoxW(h,wideError(e).c_str(),L"提示词",MB_ICONERROR);if(msg==WM_CREATE)return -1;}
    return DefWindowProcW(h,msg,w,l);
}
std::unique_ptr<LibraryWindow> active;
}
LibraryWindow::~LibraryWindow(){for(auto f:fonts)DeleteObject(f);DeleteObject(paperBrush);DeleteObject(railBrush);}
void LibraryWindow::create(){
    dpi=GetDpiForWindow(h);
    auto add=[&](int id,const wchar_t* cls,const wchar_t* title,DWORD style){
        HWND c=CreateWindowExW(0,cls,title,WS_CHILD|WS_TABSTOP|style,0,0,10,10,h,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
        if(!c)throw std::runtime_error("无法创建窗口控件。");
        if(!wcscmp(cls,L"EDIT")){SendMessageW(c,EM_SETLIMITTEXT,0x7ffffffe,0);SendMessageW(c,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(2,2));}
        return c;
    };
    auto btn=[&](int id,const wchar_t* s){return add(id,L"BUTTON",s,BS_OWNERDRAW);};
    add(Search,L"EDIT",L"",ES_AUTOHSCROLL);SendMessageW(item(Search),EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索标题或正文");
    btn(FilterAll,L"全部");for(int i=0;i<4;i++)btn(FilterFirst+i,L"");btn(FilterMore,L"更多分类");
    auto list=add(List,L"LISTBOX",L"提示词列表",LBS_NODATA|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_NOINTEGRALHEIGHT|WS_VSCROLL);
    SetWindowSubclass(list,rowMouse,1,(DWORD_PTR)this);
    btn(New,L"＋ 新建");btn(Edit,L"编辑");btn(More,L"更多");btn(EmptyAction,L"新建第一条提示词");
    add(Body,L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL);
    add(TitleInput,L"EDIT",L"",ES_AUTOHSCROLL);add(BodyInput,L"EDIT",L"",ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL);
    add(TagInput,L"EDIT",L"",ES_AUTOHSCROLL);SendMessageW(item(TagInput),EM_SETCUEBANNER,TRUE,(LPARAM)L"输入新标签，回车添加");
    btn(TagMenu,L"选择标签");btn(Save,L"保存");btn(Cancel,L"取消");
    style();ready=true;reloadTags();reload();SetFocus(item(Search));
}
void LibraryWindow::style(){
    colors=Palette();DeleteObject(paperBrush);DeleteObject(railBrush);
    paperBrush=CreateSolidBrush(colors.paper);railBrush=CreateSolidBrush(colors.rail);
    auto old=fonts;fonts={fontAt(14,FW_NORMAL,dpi),fontAt(14,FW_SEMIBOLD,dpi),fontAt(23,FW_SEMIBOLD,dpi),fontAt(16,FW_NORMAL,dpi)};
    for(HWND c=GetWindow(h,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){
        auto id=GetDlgCtrlID(c);SendMessageW(c,WM_SETFONT,(WPARAM)fonts[(id==Body||id==BodyInput)?3:0],FALSE);}
    for(auto f:old)DeleteObject(f);
    SendMessageW(item(List),LB_SETITEMHEIGHT,0,px(46));
    COLORREF caption=colors.bar;DwmSetWindowAttribute(h,DWMWA_CAPTION_COLOR,&caption,sizeof(caption));
}
void LibraryWindow::layout(){
    RECT client;GetClientRect(h,&client);width=MulDiv(client.right,96,dpi);height=MulDiv(client.bottom,96,dpi);
    split=std::clamp(width*35/100,234,344);int x=split+26,rw=width-x-26;
    auto move=[&](int id,int a,int b,int c,int d,bool visible){
        HWND ctrl=item(id);
        if(visible){SetWindowPos(ctrl,nullptr,px(a),px(b),std::max(1,px(c)),std::max(1,px(d)),SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);}
        else ShowWindow(ctrl,SW_HIDE);
    };
    move(Search,44,88,split-75,23,true);
    int tx=18;
    auto chip=[&](int id,const std::wstring& name,int cw){SetWindowTextW(item(id),name.c_str());move(id,tx,130,cw,28,true);tx+=cw+5;};
    chip(FilterAll,L"全部",48);visibleTags=0;
    HDC dc=GetDC(h);auto old=SelectObject(dc,fonts[0]);
    for(int i=0;i<4;i++){
        if(i<(int)tags.size()){
            SIZE size;GetTextExtentPoint32W(dc,tags[i].name.c_str(),tags[i].name.size(),&size);
            int cw=std::clamp(MulDiv(size.cx,96,dpi)+20,42,104);
            bool more=i+1<(int)tags.size();
            if(visibleTags==i&&tx+cw+(more?39:0)<=split-14){chip(FilterFirst+i,tags[i].name,cw);++visibleTags;continue;}
        }ShowWindow(item(FilterFirst+i),SW_HIDE);
    }
    SelectObject(dc,old);ReleaseDC(h,dc);
    SetWindowTextW(item(FilterMore),L"···");move(FilterMore,tx,130,33,28,visibleTags<(int)tags.size());
    move(List,8,176,split-16,height-189,true);
    move(New,width-109,13,87,35,!editing);move(Edit,width-195,13,72,35,!editing&&current.has_value());
    move(More,width-253,13,52,35,!editing&&current.has_value());
    move(Cancel,width-189,13,73,35,editing);move(Save,width-105,13,83,35,editing);
    move(Body,x,168,rw,height-190,!editing&&current.has_value());
    move(EmptyAction,x,212,std::min(rw,170),39,!editing&&!current.has_value());
    move(TitleInput,x+9,116,rw-18,25,editing);move(TagInput,x+9,184,rw-126,24,editing);
    move(TagMenu,width-133,178,104,33,editing);move(BodyInput,x+9,289,rw-18,height-320,editing);
    for(int id:std::initializer_list<int>{Search,List,FilterAll,FilterMore,FilterFirst,FilterFirst+1,FilterFirst+2,FilterFirst+3})EnableWindow(item(id),!editing);
    InvalidateRect(h,nullptr,FALSE);
}
HWND openLibrary(HWND){
    auto path=dataDir();std::filesystem::create_directories(path);
    active=std::make_unique<LibraryWindow>(path/L"library.sqlite3");
    WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(100));wc.lpszClassName=className;RegisterClassW(&wc);
    POINT cursor;GetCursorPos(&cursor);auto m=MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(m,&mi);auto r=mi.rcWork;
    UINT dpi=GetDpiForSystem();int w=std::min<int>(r.right-r.left,MulDiv(900,dpi,96)),ht=std::min<int>(r.bottom-r.top,MulDiv(580,dpi,96));
    auto h=CreateWindowExW(WS_EX_CONTROLPARENT,className,L"提示词",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,r.left+(r.right-r.left-w)/2,r.top+(r.bottom-r.top-ht)/2,w,ht,nullptr,nullptr,wc.hInstance,active.get());
    if(!h){active.reset();throw std::runtime_error("无法打开管理窗口。");}return h;
}
bool libraryKeys(HWND h,MSG& m){return active&&active->h==h&&active->keys(m);}
void releaseLibrary(){active.reset();}

