#include "ui/RowCopy.h"
#include <stdexcept>
int main(){
    RowCopyGesture g;RECT row{0,30,200,60};POINT p{185,40};
    if(!g.accepts(WM_LBUTTONDOWN,p,row,1000,500,3,3) || g.accepts(WM_LBUTTONDBLCLK,p,row,1020,500,3,3)
        || g.accepts(WM_LBUTTONDOWN,p,row,1030,500,3,3) || g.accepts(WM_LBUTTONDOWN,{185,100},row,2000,500,3,3)
        || !g.accepts(WM_LBUTTONDOWN,p,row,2100,500,3,3))throw std::runtime_error("copy hit testing");
}
