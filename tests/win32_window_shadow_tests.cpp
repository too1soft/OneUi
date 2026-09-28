#include "platform/win32/window_shadow.h"

#include <cstdio>
#include <initializer_list>

namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) {
        if (failures < 20) std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

void checkWindow(int width, int height, float scale) {
    const float radius = std::ceil(8.0f * scale);
    const int margin = static_cast<int>(std::ceil(18.0f * scale));
    oneui::win32::RoundedWindowShadow shadow(width, height, radius,
                                            static_cast<float>(margin), 3.0f * scale, 66);
    // Every straight section must meet the first pixel below the actual window,
    // including the former three logical pixels of transparent desktop.
    for (int x = static_cast<int>(radius); x < width - radius; ++x) {
        int previous = 66;
        for (int offset = 0; offset < margin; ++offset) {
            const int alpha = shadow.alphaAt(x + 0.5f, height + offset + 0.5f);
            check(alpha <= previous, "bottom shadow fades continuously away from the window");
            if (offset < static_cast<int>(std::ceil(3.0f * scale))) {
                check(alpha > 0, "no transparent strip at the bottom edge");
            }
            previous = alpha;
        }
    }
    for (int y = 0; y < height; ++y) {
        check(shadow.alphaAt(width * 0.5f, y + 0.5f) == 0,
              "main window interior stays transparent, including the top three rows");
    }
    check(shadow.alphaAt(width * 0.5f, -0.5f) > 0, "top edge remains softly shadowed");
    check(shadow.alphaAt(-0.5f, height * 0.5f) > 0, "left edge has no gap");
    check(shadow.alphaAt(width + 0.5f, height * 0.5f) > 0, "right edge has no gap");
    check(shadow.alphaAt(0.5f, height - 0.5f) > 0, "bottom corner outside the rounded window is shadowed");
    check(shadow.alphaAt(radius, height - radius) == 0, "inside rounded corner remains transparent");
    check(shadow.alphaAt(width * 0.5f, height + margin + 3 * scale + 0.5f) == 0,
          "shadow fades fully to transparent outside its spread");
    check(shadow.alphaAt(width * 0.5f, height + 0.5f) > shadow.alphaAt(width * 0.5f, -0.5f),
          "downward offset still makes the lower shadow stronger");
    for (int y = -margin; y < height + margin; y += 3) {
        check(shadow.alphaAt(-0.5f, y + 0.5f) == shadow.alphaAt(width + 0.5f, y + 0.5f),
              "left and right falloff are symmetric");
    }
    std::printf("Checked %dx%d at %.0f%%; bottom alpha:", width, height, scale * 100);
    for (int offset = 0; offset < 8; ++offset) {
        std::printf(" %u", static_cast<unsigned>(shadow.alphaAt(width * 0.5f, height + offset + 0.5f)));
    }
    std::printf("\n");
}
}

int main() {
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
        for (int width : {320, 1200, 1441}) {
            checkWindow(static_cast<int>(std::ceil(width * scale)),
                        static_cast<int>(std::ceil((width == 320 ? 240 : 760) * scale)), scale);
        }
    }
    oneui::win32::RoundedWindowShadow square(320, 240, 0, 18, 3, 66);
    check(square.alphaAt(0.5f, 0.5f) == 0, "zero-radius interior remains clear");
    check(square.alphaAt(160.0f, 240.5f) > 0, "zero-radius bottom edge has no gap");
    std::printf("Failures: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
