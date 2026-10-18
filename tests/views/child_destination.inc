#include "../support/recording_canvas.h"

// Exercise the real Widget/View tree with a deterministic native Window host.
// Platform HWND focus routing remains part of the application's native probe.
void childDestinationContract(const char* path) {
    ApiCase parent(path), child(path);
    parent.start();
    auto name = [](const char* s) { return OneUiUtf8String{s, std::char_traits<char>::length(s)}; };
    // A Surface lays children out in its padded content box, not its outer
    // frame. Fix a nonzero inset so this contract does not track theme tokens.
    const auto css = parent.mount->value->externalCss + "\nSurface { padding: 11px; gap: 0px; }";
    expect(oneui_view_mount_set_styles(parent.mount, {css.data(), css.size()}, 0) == 0,
           "geometry fixture specifies Surface content inset");
    auto parentRoot = std::dynamic_pointer_cast<oneui::View>(parent.view->value->root->element.widget);
    std::shared_ptr<Instance> slot;
    for (const auto& instance : parent.view->value->root->children)
        if (instance->element.widget->accessibleName() == L"child-slot") {
            expect(!slot, "child-slot name must be unique");
            slot = instance;
        }
    expect(slot && slot->element.component == "Surface", "named child-slot is the declared Surface");
    auto surface = std::dynamic_pointer_cast<oneui::View>(slot->element.widget);
    expect(bool(surface), "declared Surface is a native View container");
    OneUiWidget destination;
    auto panel = std::make_shared<oneui::Panel>();
    destination.widget = panel;
    OneUiViewMount* mount = nullptr;
    auto create = [&](OneUiWidget* widget) {
        return oneui_view_mount_child_with_destination(parent.mount, name("child-slot"), widget, &mount);
    };
    expect(create(nullptr) == ONEUI_VIEW_INVALID_ARGUMENT && !mount, "destination is mandatory");
    OneUiWidget wrongType;
    wrongType.widget = std::make_shared<oneui::View>();
    expect(create(&wrongType) == ONEUI_VIEW_TYPE && !mount, "destination requires Panel");
    OneUiWidget self;
    self.widget = surface;
    expect(create(&self) == ONEUI_VIEW_INVALID_ARGUMENT && !mount, "destination rejects self cycle");
    panel->add(parentRoot);
    expect(create(&destination) == ONEUI_VIEW_INVALID_ARGUMENT && !mount,
           "destination rejects ancestor cycle before mutation");
    panel->clearChildren();
    panel->add(std::make_shared<oneui::Panel>());
    expect(create(&destination) == ONEUI_VIEW_INVALID_ARGUMENT && !mount,
           "destination rejects nonempty Panel");
    panel->clearChildren();
    auto foreignRoot = std::make_shared<oneui::Panel>();
    foreignRoot->add(panel);
    child.window.window->setContent(foreignRoot);
    child.window.rootContent = foreignRoot;
    expect(create(&destination) == ONEUI_VIEW_BUSY && !mount,
           "destination rejects ownership in another window tree");
    foreignRoot->clearChildren();
    child.window.window->setContent(nullptr);
    child.window.rootContent.reset();
    auto localOwner = std::make_shared<oneui::Panel>();
    parentRoot->add(localOwner);
    localOwner->add(panel);
    expect(create(&destination) == ONEUI_VIEW_BUSY && !mount,
           "destination rejects existing same-window attachment");
    localOwner->clearChildren();
    auto original = parentRoot->children();
    original.pop_back();
    parentRoot->reconcileChildren(std::move(original));

    expect(create(&destination) == 0 && mount, "unattached Panel first binds parent window");
    expect(surface->children().size() == 1 && surface->children().front() == panel &&
           mount->value->destination == panel && panel->hasCompositionOwner(),
           "mount destination is the actual attached Panel");
    OneUiViewMount* duplicate = nullptr;
    expect(oneui_view_mount_child(parent.mount, name("child-slot"), &duplicate) == ONEUI_VIEW_BUSY,
           "old child cannot reserve explicit destination slot");
    expect(oneui_view_mount_child_with_destination(parent.mount, name("child-slot"), &destination,
           &duplicate) == ONEUI_VIEW_BUSY && !duplicate, "explicit duplicate reservation rejected");
    ApiCase otherParent(path);
    otherParent.start();
    expect(oneui_view_mount_child_with_destination(otherParent.mount, name("child-slot"), &destination,
           &duplicate) == ONEUI_VIEW_BUSY && !duplicate, "Panel cannot mount into two parent windows");

    OneUiView* view = nullptr;
    auto instantiate = [&] {
        return oneui_view_instantiate(mount, child.bytes.data(), child.bytes.size(),
                                     &child.vm.table, &child.vm, &view);
    };
    expect(instantiate() == 0, "explicit destination instantiates independent View");
    auto childRoot = std::dynamic_pointer_cast<oneui::View>(view->value->root->element.widget);
    expect(panel->children().size() == 1 && panel->children().front() == childRoot,
           "child declarative root remains inside supplied Panel");
    auto input = uniqueChild(view->value->root, "Input")->element.widget;
    expect(bool(std::dynamic_pointer_cast<oneui::TextField>(input)), "unique Input is a native TextField");
    input->setAccessibleName(L"destination-focus");
    const int parentFlushed = oneui_view_flush(parent.view);
    const int childFlushed = oneui_view_flush(view);
    std::cout << "destination geometry before paint: parent.flush=" << parentFlushed
              << " child.flush=" << childFlushed << " configured Surface padding=11 gap=0\n";
    expect(parentFlushed == 0 && childFlushed == 0, "geometry fixture flushes both live views");
    std::vector<oneui::Size> sizes;
    panel->setOnSizeChanged([&](oneui::Size value) { sizes.push_back(value); });
    auto paint = [&](float width, float height) {
        parentRoot->setFrame({0, 0, width, height});
        oneui::test_support::RecordingCanvas canvas;
        parentRoot->paint(canvas);
    };
    auto expectContentFrame = [&] {
        const auto outer = surface->frame();
        const auto actual = panel->frame();
        const auto content = childRoot->frame();
        auto printRect = [](const char* label, oneui::Rect rect) {
            std::cout << label << "=(" << rect.x << ',' << rect.y << ','
                      << rect.width << ',' << rect.height << ") ";
        };
        printRect("Surface", outer);
        printRect("Panel", actual);
        printRect("childRoot", content);
        auto stack = std::dynamic_pointer_cast<oneui::Stack>(surface);
        std::cout << "Surface.engine=" << int(stack->engine()) << " align=" << int(stack->align())
                  << " gap=" << stack->gap() << " panel.explicitFlex="
                  << bool(stack->explicitFlex(panel)) << " panel.preferred=("
                  << panel->preferredSize().width << ',' << panel->preferredSize().height << ")\n";
        expect(actual.width > 0 && actual.height > 0 &&
               actual.x == outer.x + 11 && actual.y == outer.y + 11 &&
               actual.width == outer.width - 22 && actual.height == outer.height - 22 &&
               content.x == actual.x && content.y == actual.y &&
               content.width == actual.width && content.height == actual.height,
               "supplied root and child frames exactly match the Surface content box");
    };
    paint(800, 900);
    expectContentFrame();
    const auto firstWidth = panel->frame().width;
    paint(1100, 1200);
    expectContentFrame();
    expect(sizes.size() >= 2 && panel->frame().width > firstWidth &&
           sizes.back().width == panel->frame().width &&
           childRoot->frame().height == panel->frame().height,
           "resize updates retained Panel observer and declarative content");
    expect(parentRoot->requestFocus(input.get()) && panel->focused() && input->focused(),
           "supplied Panel participates in native descendant focus chain");
    int focused = 0;
    expect(oneui_view_mount_focus_element(mount, name("destination-focus"), 1, &focused) == 0 &&
           focused && static_cast<Window*>(parent.window.window.get())->focused == input.get(),
           "child focus request uses parent Window");
    expect(oneui_view_flush(parent.view) == 0 && oneui_view_flush(view) == 0 &&
           surface->children().front() == panel, "ordinary flush retains Panel identity");
    panel->setOnSizeChanged({});

    oneui_view_destroy(view); view = nullptr;
    expect(panel->children().empty() && surface->children().size() == 1,
           "destroyed child View leaves exactly one reserved empty Panel");
    expect(instantiate() == 0 && panel->children().size() == 1,
           "same mount can instantiate again without duplicate nodes");
    oneui_view_destroy(view); view = nullptr;
    oneui_view_mount_destroy(mount); mount = nullptr;
    expect(surface->children().empty() && !panel->hasCompositionOwner(),
           "destroying child mount removes Panel and callbacks");
    expect(create(&destination) == 0 && instantiate() == 0,
           "detached Panel can remount without stale reservation");
    expect(oneui_view_reload(parent.view, parent.bytes.data(), parent.bytes.size()) == 0,
           "parent reload closes explicit destination child");
    expect(oneui_view_flush(view) == ONEUI_VIEW_CLOSED && surface->children().empty() &&
           panel->children().empty() && !panel->hasCompositionOwner(),
           "reload removes owned Panel before retiring parent tree");
    expect(oneui_view_mount_set_styles(mount, name(""), 0) == ONEUI_VIEW_CLOSED,
           "old explicit child mount invalid after reload");
    oneui_view_destroy(view); view = nullptr;
    oneui_view_mount_destroy(mount); mount = nullptr;
    expect(create(&destination) == 0 && instantiate() == 0, "Panel rebinds after parent reload");
    // Native shared ownership survives release of the caller's Widget handle.
    destination.widget.reset();
    expect(oneui_view_flush(view) == 0, "child owns destination independently of Widget handle");
    oneui_view_destroy(parent.view); parent.view = nullptr;
    expect(oneui_view_flush(view) == ONEUI_VIEW_CLOSED && panel->children().empty() &&
           !panel->hasCompositionOwner(), "parent destruction invalidates and detaches child");
    oneui_view_destroy(view);
    oneui_view_mount_destroy(mount);
    expect(child.vm.retains == child.vm.releases && child.vm.disconnects == 4,
           "all destination child VM lifetimes release exactly once");
}

void childDestinationCloseContract(const char* path) {
    ApiCase parent(path);
    parent.start();
    OneUiWidget destination;
    auto panel = std::make_shared<oneui::Panel>();
    destination.widget = panel;
    OneUiUtf8String name{"child-slot", 10};
    OneUiViewMount* mount = nullptr;
    expect(oneui_view_mount_child_with_destination(parent.mount, name, &destination, nullptr)
           == ONEUI_VIEW_INVALID_ARGUMENT && !panel->hasCompositionOwner(),
           "missing output does not attach destination");
    int result = 0;
    std::thread worker([&] {
        result = oneui_view_mount_child_with_destination(parent.mount, name, &destination, &mount);
    });
    worker.join();
    expect(result == ONEUI_VIEW_WRONG_THREAD && !mount && !panel->hasCompositionOwner(),
           "destination creation requires parent UI thread");
    expect(oneui_view_mount_child_with_destination(parent.mount, name, &destination, &mount) == 0,
           "uninstantiated destination reserves successfully");
    auto surface = mount->value->childSurface.lock();
    surface->reconcileChildren({});
    expect(oneui_view_mount_set_styles(mount, {nullptr, 0}, 0) == ONEUI_VIEW_CLOSED,
           "externally detached destination invalidates mount operations");
    ApiCase otherParent(path);
    otherParent.start();
    OneUiViewMount* duplicate = nullptr;
    expect(oneui_view_mount_child_with_destination(otherParent.mount, name, &destination, &duplicate)
           == ONEUI_VIEW_BUSY && !duplicate,
           "detaching a reserved Panel cannot bypass duplicate mount protection");
    oneui_view_mount_destroy(mount); mount = nullptr;
    expect(!panel->hasCompositionOwner(), "uninstantiated mount destruction detaches Panel");
    expect(oneui_view_mount_child_with_destination(parent.mount, name, &destination, &mount) == 0,
           "uninstantiated slot reservation can repeat");
    parent.window.window->close();
    expect(!panel->hasCompositionOwner() && panel->children().empty() &&
           oneui_view_mount_set_styles(mount, {nullptr, 0}, 0) == ONEUI_VIEW_CLOSED,
           "window close invalidates and detaches uninstantiated child mount");
    oneui_view_mount_destroy(mount);
}
