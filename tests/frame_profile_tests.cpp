#include "../src/internal/frame_profile.h"
#include <chrono>
#include <thread>
#include <iostream>

int main() {
    using namespace oneui::internal;
    int failures = 0;
    auto check = [&](bool ok) { if (!ok) ++failures; };
    check(activeFrameProfile == nullptr);
    {
        FrameProfileSession session(true);
        {
            FrameSpan outer(FrameStage::Layout);
            {
                FrameSpan inner(FrameStage::Layout);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            check(session.data.depth[0] == 1 && session.data.ms[0] == 0);
            {
                FrameProfileSession disabled(false);
                FrameSpan ignored(FrameStage::Fill);
                check(activeFrameProfile == nullptr);
            }
            check(activeFrameProfile == &session.data);
        }
        check(session.data.depth[0] == 0 && session.data.ms[0] > 0);
        check(session.data.ms[2] == 0);
    }
    check(activeFrameProfile == nullptr);
    if (failures) std::cerr << failures << " frame profile checks failed\n";
    return failures ? 1 : 0;
}
