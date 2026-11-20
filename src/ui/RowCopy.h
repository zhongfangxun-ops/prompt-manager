#pragma once
#include <windows.h>
#include <cstdlib>
struct RowCopyGesture {
    bool previous=false;
    DWORD lastTime{};
    POINT lastPoint{};
    bool accepts(UINT message, POINT point, RECT actualRow, DWORD now, DWORD doubleClickTime, int toleranceX, int toleranceY) {
        if(!PtInRect(&actualRow, point) || message==WM_LBUTTONDBLCLK)return false;
        bool repeated=previous && now-lastTime<=doubleClickTime &&
            std::abs(point.x-lastPoint.x)<=toleranceX && std::abs(point.y-lastPoint.y)<=toleranceY;
        previous=true;lastTime=now;lastPoint=point;
        return !repeated;
    }
};
