#include "ui/PromptEditor.h"
#include "platform/Clipboard.h"
#include <algorithm>
void EditorState::add(std::wstring name){
    name=trimmed(name);
    if(!name.empty() && std::none_of(tags.begin(),tags.end(),[&](const auto& t){return CompareStringOrdinal(t.c_str(),-1,name.c_str(),-1,TRUE)==CSTR_EQUAL;}))
        tags.push_back(name);
}
bool EditorState::changed(std::wstring_view title,std::wstring_view body,std::wstring_view pendingTag)const{
    return initial.title!=title||windowsLines(initial.body)!=body||initial.tags!=tags||!trimmed(pendingTag).empty();
}
