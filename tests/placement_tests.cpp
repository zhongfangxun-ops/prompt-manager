#include "shell/Placement.h"
#include <stdexcept>
int main() {
    if (!keepsFullscreenLauncher(L"C:/Program Files/Google/Chrome/Application/CHROME.EXE") ||
        !keepsFullscreenLauncher(LR"(C:\Apps\ChatGPT.exe)") ||
        keepsFullscreenLauncher(L"chrome.exe.bak") || keepsFullscreenLauncher(L"msedge.exe"))
        throw std::runtime_error("fullscreen launcher application names");
    RECT screen{0, 0, 2560, 1600};
    if (coversMonitor({-11, -11, 2571, 1611}, screen, WS_OVERLAPPEDWINDOW, false) ||
        !coversMonitor(screen, screen, WS_POPUP, false))
        throw std::runtime_error("framed desktop overscan");
    RECT area{-1440, 0, 0, 860};
    auto p = resolveAnchor(area, {1, 12000}, 40, 60);
    if (p.x != -40 || p.y != 800 || shouldShow(false, false) || shouldShow(true, true) ||
        !shouldShow(true, false))
        throw std::runtime_error("placement");
    auto a = nearestAnchor(area, {-1440, 400}, 40, 60);
    if (a.side != 0 || a.fraction != 5000)
        throw std::runtime_error("monitor anchor");
}
