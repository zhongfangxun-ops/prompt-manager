#include "ui/LibraryWindow.h"
#include "platform/Clipboard.h"
namespace {
int atControl(HWND owner,HWND control,HMENU menu){
    RECT r;GetWindowRect(control,&r);
    return TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN,r.left,r.bottom,0,owner,nullptr);
}
bool has(const std::vector<std::wstring>& names,const std::wstring& name){
    return std::any_of(names.begin(),names.end(),[&](const auto& s){return CompareStringOrdinal(s.c_str(),-1,name.c_str(),-1,TRUE)==CSTR_EQUAL;});
}
std::wstring menuLabel(std::wstring name){size_t i=0;while((i=name.find(L'&',i))!=name.npos){name.insert(i,L"&");i+=2;}return name;}
}
void LibraryWindow::reloadTags(){
    tags=store.tags();
    if(std::none_of(tags.begin(),tags.end(),[&](auto& t){return t.id==category;}))category=0;
}
void LibraryWindow::showDetail(){
    SetWindowTextW(item(Body),current?windowsLines(current->body).c_str():L"");
    SetWindowTextW(item(EmptyAction),category||!valueOf(item(Search)).empty()?L"清除筛选":L"新建第一条提示词");
    layout();InvalidateRect(item(List),nullptr,TRUE);
}
void LibraryWindow::pick(int i){
    current=i>=0&&i<(int)rows.size()?store.get(rows[i].id):std::nullopt;showDetail();
}
void LibraryWindow::reload(Id preferred){
    if(!preferred&&current)preferred=current->id;
    rows=store.query(category,valueOf(item(Search)));HWND list=item(List);
    SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_SETCOUNT,rows.size(),0);
    int index=0;for(size_t i=0;i<rows.size();i++)if(rows[i].id==preferred){index=i;break;}
    SendMessageW(list,LB_SETCURSEL,rows.empty()?-1:index,0);SendMessageW(list,WM_SETREDRAW,TRUE,0);pick(rows.empty()?-1:index);
}
void LibraryWindow::filter(Id id){category=id;reload();}
void LibraryWindow::filterMenu(){
    HMENU menu=CreatePopupMenu();for(size_t i=0;i<tags.size();i++)AppendMenuW(menu,MF_STRING|(tags[i].id==category?MF_CHECKED:0),i+1,menuLabel(tags[i].name).c_str());
    int k=atControl(h,item(FilterMore),menu);DestroyMenu(menu);if(k)filter(tags.at(k-1).id);
}
void LibraryWindow::edit(bool fresh){
    if(!fresh&&!current)return;
    editor.start(fresh?Prompt{0,L"",L"",{}}:*current);editing=true;
    SetWindowTextW(item(TitleInput),editor.initial.title.c_str());SetWindowTextW(item(BodyInput),windowsLines(editor.initial.body).c_str());
    SetWindowTextW(item(TagInput),L"");notice.clear();layout();SetFocus(item(TitleInput));
}
void LibraryWindow::chooseTag(){
    editor.add(valueOf(item(TagInput)));SetWindowTextW(item(TagInput),L"");
    auto options=editor.tags;for(auto& t:tags)if(!has(options,t.name))options.push_back(t.name);
    if(!options.empty()){
        HMENU menu=CreatePopupMenu();
        for(size_t i=0;i<options.size();i++)AppendMenuW(menu,MF_STRING|(has(editor.tags,options[i])?MF_CHECKED:0),i+1,menuLabel(options[i]).c_str());
        int k=atControl(h,item(TagMenu),menu);DestroyMenu(menu);
        if(k){auto name=options.at(k-1);if(has(editor.tags,name))std::erase(editor.tags,name);else editor.add(name);}
    }else SetFocus(item(TagInput));
    InvalidateRect(h,nullptr,FALSE);
}
bool LibraryWindow::save(){
    auto body=valueOf(item(BodyInput)),title=valueOf(item(TitleInput));
    if(trimmed(title).empty()||trimmed(body).empty()){
        MessageBoxW(h,L"请填写标题和正文。",L"保存提示词",MB_ICONINFORMATION);SetFocus(item(trimmed(title).empty()?TitleInput:BodyInput));return false;}
    auto finalTags=editor;finalTags.add(valueOf(item(TagInput)));
    Id id=store.save({editor.initial.id,title,body,finalTags.tags});
    editing=false;reloadTags();reload(id);
    if(!current||current->id!=id){loading=true;category=0;SetWindowTextW(item(Search),L"");loading=false;reload(id);}
    SetFocus(item(List));return true;
}
bool LibraryWindow::leaveEdit(){
    if(!editor.changed(valueOf(item(TitleInput)),valueOf(item(BodyInput)),valueOf(item(TagInput))))return true;
    TASKDIALOG_BUTTON buttons[]={{IDYES,L"保存"},{IDNO,L"放弃更改"},{IDCANCEL,L"继续编辑"}};
    TASKDIALOGCONFIG cfg{sizeof(cfg)};cfg.hwndParent=h;cfg.pszWindowTitle=L"提示词";cfg.pszMainInstruction=L"保存对提示词的更改？";
    cfg.dwFlags=TDF_ALLOW_DIALOG_CANCELLATION|TDF_POSITION_RELATIVE_TO_WINDOW;cfg.pButtons=buttons;cfg.cButtons=3;cfg.nDefaultButton=IDCANCEL;
    int result=IDCANCEL;if(FAILED(TaskDialogIndirect(&cfg,&result,nullptr,nullptr)))return false;
    return result==IDYES?save():result==IDNO;
}
void LibraryWindow::cancelEdit(){if(leaveEdit()){editing=false;editor={};showDetail();SetFocus(item(Search));}}
void LibraryWindow::flash(std::wstring text,Id id){
    notice=std::move(text);copied=id;SetTimer(h,9,1500,nullptr);InvalidateRect(h,nullptr,FALSE);InvalidateRect(item(List),nullptr,FALSE);
}
void LibraryWindow::copy(Id id){
    auto target=store.get(id);if(!target)return;
    if(!copyBody(h,target->body)){flash(L"复制失败，请重试");return;}
    try{store.markUsed(id,utcMillis());reload(current?current->id:id);}catch(...){flash(L"已复制，最近使用时间未更新",id);return;}
    flash(L"已复制",id);
}
void LibraryWindow::deleteMenu(){
    if(!current)return;
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"复制正文");AppendMenuW(menu,MF_STRING,2,L"删除提示词…");
    int k=atControl(h,item(More),menu);DestroyMenu(menu);
    if(k==1)copy(current->id);
    if(k==2&&MessageBoxW(h,(L"删除「"+current->title+L"」？").c_str(),L"删除提示词",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES){
        store.erase(current->id);reloadTags();reload();}
}
void LibraryWindow::command(int id,int code){
    if(id==Search&&code==EN_CHANGE&&!editing){reload();return;}
    if(id==List&&code==LBN_SELCHANGE&&!editing){pick(SendMessageW(item(List),LB_GETCURSEL,0,0));return;}
    if(code!=BN_CLICKED)return;
    if(editing){
        switch(id){case TagMenu:chooseTag();break;case Save:save();break;case Cancel:cancelEdit();break;}return;
    }
    switch(id){
    case FilterAll:filter(0);break;
    case FilterMore:filterMenu();break;
    case New:edit(true);break;case Edit:edit(false);break;case More:deleteMenu();break;
    case EmptyAction:
        if(category||!valueOf(item(Search)).empty()){category=0;SetWindowTextW(item(Search),L"");reload();SetFocus(item(Search));}else edit(true);break;
    default:if(id>=FilterFirst&&id<FilterFirst+visibleTags)filter(tags.at(id-FilterFirst).id);
    }
}
bool LibraryWindow::keys(MSG& m){
    if(m.hwnd!=h&&!IsChild(h,m.hwnd))return false;
    try{
        if(m.message==WM_KEYDOWN){
            bool ctrl=GetKeyState(VK_CONTROL)<0;
            if(m.wParam==VK_ESCAPE){if(editing)cancelEdit();else SendMessageW(h,WM_CLOSE,0,0);return true;}
            if(ctrl&&m.wParam=='S'&&editing){save();return true;}
            if(ctrl&&m.wParam=='N'&&!editing){edit(true);return true;}
            if(ctrl&&m.wParam=='F'&&!editing){SetFocus(item(Search));SendMessageW(item(Search),EM_SETSEL,0,-1);return true;}
            if(ctrl&&m.wParam=='A'&&m.hwnd!=item(List)){wchar_t cls[16];GetClassNameW(m.hwnd,cls,16);if(!wcscmp(cls,L"Edit")){SendMessageW(m.hwnd,EM_SETSEL,0,-1);return true;}}
            if(m.wParam==VK_RETURN&&editing&&m.hwnd==item(TagInput)){
                editor.add(valueOf(item(TagInput)));SetWindowTextW(item(TagInput),L"");InvalidateRect(h,nullptr,FALSE);return true;}
            if(!editing&&m.hwnd==item(List)&&current&&(m.wParam==VK_RETURN||(ctrl&&m.wParam=='C'))){copy(current->id);return true;}
            if(m.wParam==VK_TAB){HWND next=GetNextDlgTabItem(h,m.hwnd,GetKeyState(VK_SHIFT)<0);if(next)SetFocus(next);return true;}
        }
    }catch(const std::exception& e){MessageBoxW(h,wideError(e).c_str(),L"提示词",MB_ICONERROR);return true;}
    return false;
}
