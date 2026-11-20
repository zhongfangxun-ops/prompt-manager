#include "shell/Placement.h"
#include <stdexcept>
int main(){
    RECT area{-1440,0,0,860};
    auto p=resolveAnchor(area,{1,12000},40,60);
    if(p.x!=-40 || p.y!=800 || shouldShow(false,false) || shouldShow(true,true) || !shouldShow(true,false)) throw std::runtime_error("placement");
    auto a=nearestAnchor(area,{-1440,400},40,60);
    if(a.side!=0 || a.fraction!=5000) throw std::runtime_error("monitor anchor");
}
