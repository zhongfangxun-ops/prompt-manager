#include "shell/EdgeArtwork.h"
#include <fstream>
#include <vector>
#include <stdexcept>
int main() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token{};
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
        return 1;
    for (bool hot : {false, true}) {
        std::vector<BYTE> pixels(72 * 104 * 4);
        drawCapsule(pixels.data(), 72, 104, hot);
        if (pixels[3] != 0 || pixels[(52 * 72 + 36) * 4 + 3] < 180)
            throw std::runtime_error("capsule alpha");
        std::ofstream out(hot ? "edge-hover.bgra" : "edge.bgra", std::ios::binary);
        out.write((const char *)pixels.data(), pixels.size());
    }
    Gdiplus::GdiplusShutdown(token);
}
