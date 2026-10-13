#include "fixture.h"
#include <filesystem>
using namespace viewtest;

namespace {
struct LegacyCase {
    oneview::Image image;
    Vm vm, a, b;
    std::shared_ptr<Runtime> runtime;
    explicit LegacyCase(const std::filesystem::path& file)
        : image(load(file.string().c_str())), vm(image, "Root"),
          a(image, "Item"), b(image, "Item"), runtime(std::make_shared<Runtime>(image)) {
        a.text(a.key, "a"); b.text(b.key, "b");
        a.text(id(image, "Item", "title"), "Alpha");
        b.text(id(image, "Item", "title"), "Beta");
        a.text(id(image, "Item", "detail"), "first");
        b.text(id(image, "Item", "detail"), "second");
        vm.items = {&a, &b};
        vm.values[id(image, "Root", "index")].integer = 0;
        vm.values[id(image, "Root", "multi")].integer = 1;
        vm.values[id(image, "Root", "width")].number = 36;
        vm.values[id(image, "Root", "gap")].number = 4;
        vm.values[id(image, "Root", "click")].integer = 1;
        runtime->initialize(&vm.table, &vm);
    }
    ~LegacyCase() { runtime->close(); }
    std::shared_ptr<oneui::VirtualList> list() const {
        auto value = runtime->root->element.as<oneui::VirtualList>();
        expect(bool(value), "old IR must instantiate the native VirtualList");
        return value;
    }
    void notify(const char* property) {
        runtime->notify(runtime->rootScope, id(image, "Root", property));
        runtime->flush();
    }
};

void click(const std::shared_ptr<oneui::VirtualList>& list, int row, bool control = false) {
    list->setFrame({0, 0, 320, 96});
    oneui::MouseEvent event;
    event.position = {20, row * 32.0f + 12};
    event.button = oneui::MouseButton::Left;
    event.control = control;
    list->onMouseDown(event);
    list->onMouseUp(event);
}

void contractRejectsTampering(const oneview::Image& image) {
    expect(image.components.size() == 1 && image.components[0].name == "VirtualList" &&
           image.components[0].version == 2, "fixtures must be actual old VirtualList-only v2 images");
    auto rejected = [&](const std::function<void(oneview::ComponentContract&)>& change) {
        auto copy = image;
        change(copy.components[0]);
        bool failed = false;
        try {
            // Recompute the image checksum: failure must be contract validation,
            // not the incidental stale hash of a modified byte array.
            (void)oneview::readImage(oneview::writeImage(copy));
        } catch (const oneview::Error&) { failed = true; }
        expect(failed, "tampered historical COMP must be rejected even with a valid hash");
    };
    rejected([](auto& c) { c.name = "UnknownList"; });
    rejected([](auto& c) { c.version = 3; });
    rejected([](auto& c) { c.version = 999; });
    rejected([](auto& c) { c.properties.pop_back(); });
    for (size_t i = 0; i < image.components[0].properties.size(); ++i) {
        rejected([i](auto& c) { ++c.properties[i].id; });
        rejected([i](auto& c) { c.properties[i].name += "-changed"; });
        rejected([i](auto& c) { c.properties[i].tag ^= 1; });
        rejected([i](auto& c) { c.properties[i].flags ^= 1; });
        rejected([i](auto& c) { c.properties[i].record += "-changed"; });
    }
    rejected([](auto& c) { c.events.pop_back(); });
    for (size_t i = 0; i < image.components[0].events.size(); ++i) {
        rejected([i](auto& c) { ++c.events[i].id; });
        rejected([i](auto& c) { c.events[i].name += "-changed"; });
        rejected([i](auto& c) { c.events[i].args.push_back({"extra", 4}); });
        for (size_t a = 0; a < image.components[0].events[i].args.size(); ++a) {
            rejected([i, a](auto& c) { c.events[i].args[a].name += "-changed"; });
            rejected([i, a](auto& c) { c.events[i].args[a].tag ^= 1; });
        }
    }
    rejected([](auto& c) { c.childRule = "many"; });
    rejected([](auto& c) { c.modelProperty = 9; });
}

void exercise(const std::filesystem::path& directory, const std::string& branch,
              const std::string& variant) {
    LegacyCase c(directory / (branch + variant + ".oneview"));
    const bool terminal = branch == "Terminal", bound = variant != "Static";
    const auto& contract = c.image.components[0];
    expect(contract.property(15).name == (terminal ? "multiple" : "trailing-width"),
           "old ID15 retains its image-local meaning");
    auto list = c.list();
    expect(list->items().size() == 2 && list->rowHeight() == 32,
           "old static and list bindings initialize native rows");
    expect(!c.image.schema.model("Item").properties.count("selected"),
           "old list projection does not require optional selected");
    if (variant == "Call")
        expect(c.image.count(oneview::CALL) != 0, "old CALL fixture must contain a real imported component");
    if (terminal) {
        expect(list->selectionMode() == oneui::SelectionMode::Multiple, "old terminal ID15 means multiple");
        click(list, 1, true);
        expect(c.vm.invokedArgs.size() == 2 && c.vm.invokedArgs[0].tag == 2 &&
               c.vm.invokedArgs[0].integer == 1 && c.vm.invokedArgs[1].tag == 1 &&
               c.vm.invokedArgs[1].integer == 1, "old typed selectionChanged event retains arguments");
        if (bound) {
            c.vm.values[id(c.image, "Root", "multi")].integer = 0; c.notify("multi");
            expect(list->selectionMode() == oneui::SelectionMode::Single, "old multiple binding updates by ID");
        }
    } else {
        expect(list->selectionMode() == oneui::SelectionMode::Single &&
               list->richMetrics().trailingWidth == 36 && list->richMetrics().trailingGap == 4,
               "old tools IDs15/16 do not accidentally enable multiple");
        const auto before = c.vm.invokes; click(list, 1);
        expect(c.vm.invokes == before + 1 && c.vm.invokedArgs.empty(), "old ID17 enables single-click activation");
        if (bound) {
            c.vm.values[id(c.image, "Root", "width")].number = 44; c.notify("width");
            c.vm.values[id(c.image, "Root", "gap")].number = 6; c.notify("gap");
            c.vm.values[id(c.image, "Root", "click")].integer = 0; c.notify("click");
            expect(list->richMetrics().trailingWidth == 44 && list->richMetrics().trailingGap == 6,
                   "old tools numeric bindings update correct native metrics");
            const auto noClick = c.vm.invokes; click(list, 0);
            expect(c.vm.invokes == noClick, "old ID17 false disables single-click activation");
        }
    }
    oneui::KeyEvent down; down.key = oneui::Key::Down;
    list->setSelectedIndex(0); list->onKeyDown(down);
    expect(c.vm.values[id(c.image, "Root", "index")].integer == 1,
           "old modelProperty10 writes native selection through v-model");
    oneui::KeyEvent enter; enter.key = oneui::Key::Enter;
    const auto before = c.vm.invokes; list->onKeyDown(enter);
    expect(c.vm.invokes == before + 1 && c.vm.invokedArgs.empty(), "old activation event remains live");
    const auto resetName = branch + (variant == "Call" ? "ResetCall.oneview" : "Reset.oneview");
    const auto reset = load((directory / resetName).string().c_str());
    c.runtime->reload(reset);
    expect(c.list() == list && list->selectionMode() == oneui::SelectionMode::Single &&
           list->richMetrics().trailingWidth == 58 && list->richMetrics().trailingGap == 8,
           "removing old properties resets by name without replacing the native list");
    const auto resetInvokes = c.vm.invokes; click(list, 0);
    expect(c.vm.invokes == resetInvokes, "reload removal restores default click behavior and removes selection event");
    c.runtime->reload(c.image);
    expect(c.list() == list, "reloading old authored contract retains the native list");
    // Exercise both directions across the colliding old ID15 contracts.
    c.vm.values[id(c.image, "Root", "multi")].integer = 1;
    c.vm.values[id(c.image, "Root", "width")].number = 36;
    c.vm.values[id(c.image, "Root", "gap")].number = 4;
    c.vm.values[id(c.image, "Root", "click")].integer = 1;
    for (const auto* other : {"Tools", "Terminal"}) {
        const auto otherName = std::string(other) + (variant == "Call" ? "Call.oneview" : "Static.oneview");
        c.runtime->reload(load((directory / otherName).string().c_str()));
        expect(c.list() == list, "cross-contract reload retains the native list");
        const bool otherTerminal = std::string(other) == "Terminal";
        expect(list->selectionMode() == (otherTerminal ? oneui::SelectionMode::Multiple : oneui::SelectionMode::Single) &&
               list->richMetrics().trailingWidth == (otherTerminal ? 58 : 36),
               "cross-contract reload resets and applies colliding IDs by each image's names");
    }
    c.runtime->close();
    const auto closedInvokes = c.vm.invokes; list->onKeyDown(enter);
    expect(c.vm.invokes == closedInvokes && c.vm.retains == c.vm.releases &&
           c.a.retains == c.a.releases && c.b.retains == c.b.releases,
           "historical images preserve disconnect and lease balance");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        const auto& current = oneview::registry().at("VirtualList");
        expect(current.version == 3 && current.property(15).name == "multiple" &&
               current.property(16).name == "trailing-width" && current.property(17).name == "trailing-gap" &&
               current.property(18).name == "activate-on-single-click", "new v3 contract has unique additive IDs");
        const std::filesystem::path directory(argv[1]);
        for (const auto* branch : {"Terminal", "Tools"}) {
            contractRejectsTampering(load((directory / (std::string(branch) + "Static.oneview")).string().c_str()));
            for (const auto* variant : {"Static", "Bound", "Call"}) exercise(directory, branch, variant);
        }
        std::cout << "PASS legacy VirtualList: actual old IR, static/BIND/CALL/events/model/reload/tamper rejection\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
