#include "oneui/ui_declarative.h"
#include "oneui/controls/button.h"
#include "oneui/controls/label.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>

struct InstallerModel {
    oneui::State<std::string> title{"安装准备就绪 / Ready — العربية 👋"};
    oneui::State<std::string> path{"C:/Program Files/Example"};
    oneui::State<bool> consent{true};
    oneui::State<double> progress{0.5};
    int installed = 0;
    struct Install { InstallerModel* owner; void execute() { ++owner->installed; } } install{this};
};
#include "Installer.g.h"

using Clock = std::chrono::steady_clock;
static double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
static void require(bool valid, const char* message) {
    if (!valid) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        const auto started = Clock::now();
        auto model = std::make_shared<InstallerModel>();
        oneui::ui::Mount mount;
        auto root = build_Installer(model, mount);
        root.widget->setFrame({0, 0, 640, 400});
        auto title = std::dynamic_pointer_cast<oneui::Label>(mount.find("title"));
        require(title && !title->text().empty(), "initial text");
        auto button = std::dynamic_pointer_cast<oneui::Button>(mount.find("install"));
        require(bool(button), "button exists");
        button->onKeyDown({oneui::Key::Enter});
        require(model->installed == 1, "command callback");
        const auto startup = milliseconds(started);
        SkBitmap bitmap;
        bitmap.allocN32Pixels(640, 400);
        SkCanvas skia(bitmap);
        auto canvas = oneui::rendering::makeSkiaCanvas(skia);
        skia.clear(SK_ColorWHITE);
        root.widget->paint(*canvas); // Warm caches before timed work.
        std::vector<double> paint, updates;
        for (int batch = 0; batch < 21; ++batch) {
            auto start = Clock::now();
            for (int i = 0; i < 20; ++i) root.widget->paint(*canvas);
            paint.push_back(milliseconds(start) / 20);
            start = Clock::now();
            for (int i = 0; i < 100; ++i) {
                model->title.set("状态更新 / Updated " + std::to_string(i));
                mount.flush();
            }
            updates.push_back(milliseconds(start) / 100);
        }
        require(title->text() == L"状态更新 / Updated 99", "reactive notification");
        model->title.set("安装准备就绪 / Ready — العربية 👋");
        mount.flush();
        skia.clear(SK_ColorWHITE);
        root.widget->paint(*canvas);
        if (argc == 2) {
            std::ofstream image(argv[1], std::ios::binary);
            image.write(static_cast<const char*>(bitmap.getPixels()), bitmap.computeByteSize());
            require(bool(image), "write rendered pixels");
        }
        std::sort(paint.begin(), paint.end());
        std::sort(updates.begin(), updates.end());
        std::cout << "{\"startup_ms\":" << startup
                  << ",\"paint_median_ms\":" << paint[10]
                  << ",\"paint_p95_ms\":" << paint[19]
                  << ",\"update_median_ms\":" << updates[10]
                  << ",\"update_p95_ms\":" << updates[19] << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
