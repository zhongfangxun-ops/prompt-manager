#include "ui/LibraryWindow.h"
namespace {
std::wstring joined(const std::vector<std::wstring>& tags){
    std::wstring s;for(auto& name:tags){if(!s.empty())s+=L"    /    ";s+=name;}return s;
}}
void LibraryWindow::paint(){
    PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);
    fill(dc,ps.rcPaint,colors.paper);fill(dc,rect(0,60,split,height-60),colors.rail);
    fill(dc,rect(0,0,width,60),colors.bar);
    fill(dc,rect(0,59,width,1),colors.line);fill(dc,rect(split-1,60,1,height-60),colors.line);
    auto frame=[&](int x,int y,int w,int ht){rounded(dc,rect(x,y,w,ht),px(12),colors.paper,colors.line);};
    rounded(dc,rect(19,13,34,34),px(13),colors.accent,colors.accent);
    label(dc,L"›_",rect(19,11,34,34),fonts[1],colors.reverse,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
    label(dc,L"提示词库",rect(64,9,170,42),fonts[2],colors.ink);
    frame(17,79,split-35,39);
    auto pen=CreatePen(PS_SOLID,std::max(1,px(1)),colors.muted);auto prev=SelectObject(dc,pen);auto brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    Ellipse(dc,px(26),px(92),px(36),px(102));MoveToEx(dc,px(35),px(101),nullptr);LineTo(dc,px(40),px(106));
    SelectObject(dc,brush);SelectObject(dc,prev);DeleteObject(pen);
    int x=split+26,rw=width-x-26;
    if(editing){
        label(dc,L"标题",rect(x,75,rw,28),fonts[1],colors.muted);frame(x,106,rw,42);
        label(dc,L"分类标签",rect(x,149,rw,25),fonts[1],colors.muted);frame(x,176,rw-114,36);
        label(dc,joined(editor.tags),rect(x,214,rw,27),fonts[0],colors.accent);
        label(dc,L"正文",rect(x,247,rw,28),fonts[1],colors.muted);frame(x,279,rw,height-297);
    }else if(current){
        label(dc,current->title,rect(x,82,rw,47),fonts[2],colors.ink);
        label(dc,joined(current->tags),rect(x,130,rw,27),fonts[0],colors.accent);
        fill(dc,rect(x,158,rw,1),colors.line);
    }else{
        bool filtered=category||!valueOf(item(Search)).empty();
        label(dc,filtered?L"没有匹配的提示词":L"还没有提示词",rect(x,141,rw,47),fonts[2],colors.ink);
    }
    if(!notice.empty()){
        int w=std::min(width-160,320);auto r=rect(width-w-20,height-53,w,36);rounded(dc,r,px(12),colors.tint,colors.line);
        label(dc,notice,r,fonts[1],colors.accent,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    }
    EndPaint(h,&ps);
}
bool LibraryWindow::draw(const DRAWITEMSTRUCT& d){
    HDC dc=d.hDC;RECT r=d.rcItem;
    if(d.CtlID==List){
        if(d.itemID>=rows.size())return true;
        auto& row=rows[d.itemID];bool selected=d.itemState&ODS_SELECTED;
        fill(dc,r,colors.rail);InflateRect(&r,-px(6),-px(3));
        rounded(dc,r,px(14),selected?colors.tint:colors.rail,selected?colors.line:colors.rail);
        auto title=r;title.left+=px(11);title.right-=px(58);
        label(dc,row.title,title,fonts[selected?1:0],colors.ink);
        auto copyRect=r;copyRect.left=copyRect.right-px(52);
        label(dc,copied==row.id?L"已复制":L"复制",copyRect,fonts[0],colors.accent,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if(d.itemState&ODS_FOCUS){InflateRect(&r,-px(2),-px(2));DrawFocusRect(dc,&r);}return true;
    }
    if(d.CtlType!=ODT_BUTTON)return false;
    int id=d.CtlID;bool tag=id==FilterAll||id==FilterMore||id>=FilterFirst;
    bool selected=(id==FilterAll&&category==0);
    if(id>=FilterFirst&&id<FilterFirst+(int)tags.size())selected=tags[id-FilterFirst].id==category;
    if(id==FilterMore&&category){auto it=std::find_if(tags.begin(),tags.end(),[&](auto& t){return t.id==category;});selected=it!=tags.end()&&it-tags.begin()>=visibleTags;}
    bool primary=id==New||id==Save||id==EmptyAction;
    COLORREF surround=tag?colors.rail:((id==TagMenu||id==EmptyAction)?colors.paper:colors.bar);
    fill(dc,r,surround);InflateRect(&r,-1,-1);
    auto bg=primary?colors.accent:(selected?colors.tint:(tag?colors.rail:colors.paper));
    if((d.itemState&ODS_SELECTED)&&!primary)bg=colors.tint;
    rounded(dc,r,px(tag?22:15),bg,primary?colors.accent:(selected?colors.accent:colors.line));
    label(dc,valueOf(d.hwndItem),r,fonts[primary||selected?1:0],primary?colors.reverse:colors.ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(d.itemState&ODS_FOCUS){InflateRect(&r,-px(3),-px(3));DrawFocusRect(dc,&r);}
    return true;
}
