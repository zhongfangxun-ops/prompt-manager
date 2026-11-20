#pragma once
#include "platform/App.h"
#include <algorithm>
struct Palette {
    COLORREF paper=RGB(253,253,251), rail=RGB(244,246,244),bar=RGB(234,240,237),
        ink=RGB(30,43,40), muted=RGB(82,101,94),line=RGB(212,221,215), tint=RGB(222,236,226), accent=RGB(40,86,67), reverse=RGB(255,255,255);
    Palette(){
        HIGHCONTRASTW h{sizeof(h)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(h),&h,0);
        if(h.dwFlags&HCF_HIGHCONTRASTON){paper=rail=bar=GetSysColor(COLOR_WINDOW);ink=muted=GetSysColor(COLOR_WINDOWTEXT);
            line=ink;tint=paper;accent=GetSysColor(COLOR_HIGHLIGHT);reverse=GetSysColor(COLOR_HIGHLIGHTTEXT);}
    }
};
inline void fill(HDC dc,RECT r,COLORREF color){SetDCBrushColor(dc,color);FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH));}
inline void rounded(HDC dc,RECT r,int rad,COLORREF bg,COLORREF stroke){
    auto ob=SelectObject(dc,GetStockObject(DC_BRUSH)),op=SelectObject(dc,GetStockObject(DC_PEN));
    SetDCBrushColor(dc,bg);SetDCPenColor(dc,stroke);RoundRect(dc,r.left,r.top,r.right,r.bottom,rad,rad);SelectObject(dc,op);SelectObject(dc,ob);
}
inline HFONT fontAt(int px,int weight,UINT dpi){
    return CreateFontW(-MulDiv(px,dpi,96),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
}
inline void label(HDC dc,std::wstring_view text,RECT r,HFONT font,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS){
    auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
    DrawTextW(dc,text.data(),int(text.size()),&r,flags|DT_NOPREFIX);SelectObject(dc,old);
}
inline std::wstring valueOf(HWND h){
    int n=GetWindowTextLengthW(h);std::wstring s(n+1,0);s.resize(GetWindowTextW(h,s.data(),n+1));return s;
}
