#pragma once
#include <windows.h>
#include <string>
std::wstring windowsLines(std::wstring_view);
bool copyBody(HWND owner,std::wstring_view text);
