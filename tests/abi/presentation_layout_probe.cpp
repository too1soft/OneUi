// Compile this identical source twice: once with the 64a5b89 include tree,
// once with the integrated include tree, both linked to the integrated DLL.
// Compare complete stdout; do not compile with ONEUI_BUILDING_DLL.
#include "oneui/oneui.h"
#include "oneui/controls/icon_badge.h"
#include "oneui/controls/nav_item.h"
#include "oneui/controls/pane_header.h"
#include "oneui/controls/template_table.h"
#include "oneui/layout/adaptive_panes.h"
#include "oneui/layout/dock_workspace_view.h"
#include "oneui/ui_declarative.h"
#include "oneui/ui_style_diagnostics.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <new>
#include <stdexcept>
using namespace oneui;

namespace {
void check(bool value) { if (!value) throw std::runtime_error("old-header ABI guard failed"); }
template<class T> void layout(const char* name) {
    std::cout << name << ' ' << sizeof(T) << ' ' << alignof(T) << '\n';
}
template<class T> struct Guarded {
    std::array<unsigned char, 64> before;
    alignas(T) unsigned char object[sizeof(T)];
    std::array<unsigned char, 64> after;
    Guarded() { before.fill(0x5a); after.fill(0xa5); }
    void verify() const {
        check(std::all_of(before.begin(), before.end(), [](auto v) { return v == 0x5a; }));
        check(std::all_of(after.begin(), after.end(), [](auto v) { return v == 0xa5; }));
    }
};
void oldApiCalls() {
    Guarded<Table> memory;
    std::weak_ptr<int> token;
    for (int i = 0; i < 8; ++i) {
        auto* table = new (memory.object) Table;
        token = table->lifetimeToken();
        table->setColumns({{L"A", 100}, {L"B", 200}});
        table->setRows({{L"a", L"b"}});
        table->setSelectedIndex(0);
        check(table->columns().size() == 2 && table->columns()[1].width == 200);
        check(table->rows().size() == 1 && table->selectedIndex() == 0 && !token.expired());
        memory.verify(); table->~Table(); memory.verify(); check(token.expired());
    }
    Guarded<Stack> stackMemory;
    auto* stack = new (stackMemory.object) Stack(StackDirection::Row);
    auto child = std::make_shared<Label>(L"old consumer");
    stack->add(child);
    StackFlex flex; flex.basis = 42; flex.minWidth = 5;
    stack->setFlex(child, flex);
    check(stack->explicitFlex(child)->basis == 42 && stack->explicitFlex(child)->minWidth == 5);
    StyleBox base, overlay; base.gap = 7; overlay.width = 55;
    auto merged = mergeStyleBox(base, overlay);
    check(merged.gap == 7 && merged.width == 55);
    stack->setStyleBox(merged);
    StyleSheet sheet;
    StyleRule rule; rule.selector = ".old"; rule.box = merged; sheet.addRule(rule);
    auto copy = sheet;
    check(copy.resolve({"div", {"old"}, 0}).width == 55);
    const auto bridge = computeTitleBarBridgeLayout(copy, {});
    check(bridge.buttons.size() == 3);
    stackMemory.verify(); stack->~Stack(); stackMemory.verify();
    Guarded<PaneHeader> headerMemory;
    auto* header=new(headerMemory.object) PaneHeader;
    header->setFrame({0,0,500,30});header->setStyleBox(merged);
    headerMemory.verify();header->~PaneHeader();headerMemory.verify();
    Guarded<Tabs> tabsMemory;
    auto* tabs=new(tabsMemory.object) Tabs;
    tabs->setItems({L"one",L"two"});tabs->setFrame({0,0,200,30});
    tabsMemory.verify();tabs->~Tabs();tabsMemory.verify();
    Guarded<DockWorkspaceSnapshot> snapshotMemory;
    auto* snapshot=new(snapshotMemory.object) DockWorkspaceSnapshot;
    auto item=std::make_shared<DockItemView>();item->key="pane";
    snapshot->nodes={{"leaf","pane"}};snapshot->root="leaf";snapshot->placements={{"pane","docked"}};
    DockWorkspaceView dock;dock.setSnapshot(*snapshot,{item});
    check(dock.snapshot().nodes.size()==1 && dock.snapshot().root=="leaf");
    snapshotMemory.verify();snapshot->~DockWorkspaceSnapshot();snapshotMemory.verify();
}
} // namespace

int main() {
    try {
#define LAYOUT(T) layout<T>(#T)
        LAYOUT(Widget); LAYOUT(Table); LAYOUT(StackFlex); LAYOUT(StyleBox); LAYOUT(StyleRule);
        LAYOUT(StyleSheet); LAYOUT(Card); LAYOUT(IconBadge); LAYOUT(PaneHeader); LAYOUT(Stack);
        LAYOUT(ScrollView); LAYOUT(Panel); LAYOUT(ReorderableGrid); LAYOUT(AdaptivePanes);
        LAYOUT(AppShell); LAYOUT(TopBar); LAYOUT(Sparkline); LAYOUT(ProductShellStyle);
        LAYOUT(ButtonBridgeLayout); LAYOUT(SidebarNavItemBridgeLayout);
        LAYOUT(TitleBarBridgeButton); LAYOUT(TitleBarBridgeLayout); LAYOUT(TextInputBridgeLayout);
        LAYOUT(ui::StyleDiagnostic); LAYOUT(NavItem); LAYOUT(TemplateTable);
        LAYOUT(Tabs); LAYOUT(DockWorkspaceSnapshot); LAYOUT(DockWorkspaceView);
        LAYOUT(ui::Element); LAYOUT(ui::StyleRegistry);
        LAYOUT(ui::schema::Component);
#undef LAYOUT
        for(unsigned i=0;i<78;++i) {
            const auto& component=ui::schema::component(static_cast<ui::schema::Kind>(i));
            std::cout << "Kind " << unsigned(component.kind) << ' ' << component.name << '\n';
        }
        // Compute actual public field offsets, without offsetof's standard-layout restriction.
        StyleBox box;
        auto offset = [&](const char* name, const auto& member) {
            std::cout << name << ' ' << reinterpret_cast<const char*>(&member) -
                reinterpret_cast<const char*>(&box) << '\n';
        };
        offset("StyleBox.containerName", box.containerName);
        offset("StyleBox.flexBasis", box.flexBasis);
        offset("StyleBox.background", box.background);
        offset("StyleBox.shadows", box.shadows);
        offset("StyleBox.contentBackgroundOpacity", box.contentBackgroundOpacity);
        oldApiCalls();
        std::cout << "old-header calls, placement guards and lifetime: PASS\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
