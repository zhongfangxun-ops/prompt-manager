#include "platform/App.h"
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    try { return runDesktop(); }
    catch(const std::exception& e){MessageBoxW(nullptr,wideError(e).c_str(),L"提示词",MB_ICONERROR);return 1;}
}
