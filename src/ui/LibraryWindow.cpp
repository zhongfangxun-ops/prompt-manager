#include "platform/App.h"
HWND openLibrary(HWND) {return CreateWindowW(L"STATIC",L"提示词",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,800,540,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);}
bool libraryKeys(HWND,MSG&) {return false;}
