#include "platform/Clipboard.h"
#include <cstring>
std::wstring windowsLines(std::wstring_view text){
    std::wstring out;out.reserve(text.size());
    for(size_t i=0;i<text.size();i++){
        if(text[i]==L'\r'||text[i]==L'\n'){
            if(text[i]==L'\r'&&i+1<text.size()&&text[i+1]==L'\n')++i;
            out+=L"\r\n";
        }else out+=text[i];
    }return out;
}
bool copyBody(HWND owner,std::wstring_view text){
    auto data=windowsLines(text);size_t bytes=(data.size()+1)*sizeof(wchar_t);
    HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!mem)return false;
    void* dest=GlobalLock(mem);if(!dest){GlobalFree(mem);return false;}
    memcpy(dest,data.c_str(),bytes);GlobalUnlock(mem);
    bool success=false;
    if(OpenClipboard(owner)){if(EmptyClipboard())success=SetClipboardData(CF_UNICODETEXT,mem)!=nullptr;CloseClipboard();}
    if(!success)GlobalFree(mem);
    return success;
}
