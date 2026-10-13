#include "oneui/ui_declarative.h"
#include "oneui/layout/container_query.h"
#include "core/style_layout_internal.h"
#include "core/internal/widget_presentation.h"
#include "support/recording_canvas.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace oneui;

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void cascade() {
    StyleSheet sheet;
    std::string error;
    check(sheet.addRulesFromCss(
        ":root{--basis:40%;--columns:auto 80px;}"
        "table{table-column-widths:auto 50px;table-selection-width:32px;}"
        ".row{flex-basis:20px;order:1;}"
        ".row{flex-basis:var(--basis);order:2;}"
        "table.row{table-column-widths:var(--columns);table-selection-width:40px;}"
        "@container page (width >= 600px){.row{flex-basis:60%;order:-1;}}", &error),
        "extended CSS parses with frozen StyleBox layout");
    StyleNode node{"table", {"row"}, 0};
    auto layout = sheet.resolveLayout(node);
    check(layout.flexBasisPercent == 40 && layout.flexOrder == 2 &&
          layout.tableColumnWidths == std::vector<float>({0, 80}) && layout.tableSelectionWidth == 40,
          "layout uses the same specificity and source order as ordinary CSS");
    check(!sheet.rules()[2].query && style_detail::ruleLayout(sheet.rules()[2]),
          "unconditional layout metadata does not become a container query");
    auto copy = sheet;
    copy.setCustomProperty("--basis", "25%");
    copy.setCustomProperty("--columns", "auto 96px");
    check(copy.resolveLayout(node).flexBasisPercent == 25 &&
          copy.resolveLayout(node).tableColumnWidths == std::vector<float>({0, 96}) &&
          sheet.resolveLayout(node).flexBasisPercent == 40, "sheet copies resolve independent tokens");
    copy.setCustomProperty("--basis", "invalid");
    check(copy.resolveLayout(node).flexBasisPercent == 40 && copy.resolveLayout(node).flexOrder == 2,
          "invalid dynamic token falls back to the complete initially parsed rule");
    auto effective = sheet.forContainers({{"page", {600, 300}}});
    check(effective.resolveLayout(node).flexBasisPercent == 60 &&
          effective.resolveLayout(node).flexOrder == -1, "query filtering retains extension metadata");
    for (const auto& rule : effective.rules()) check(!rule.query, "filtered query pointer remains null");
    auto narrow = sheet.forContainers({{"page", {599, 300}}});
    check(narrow.resolveLayout(node).flexBasisPercent == 40, "unmatched query does not leak extensions");
    auto moved = std::move(effective);
    check(moved.resolveLayout(node).flexOrder == -1, "moving a sheet retains owned extension state");
    check(sheet.addRulesFromCss("table.row{flex-basis:24px;}", &error), "later pixel basis parses");
    check(!sheet.resolveLayout(node).flexBasisPercent && sheet.resolve(node).flexBasis == 24,
          "later pixel basis clears an earlier percentage across rules");
    StyleSheet within;
    check(within.addRulesFromCss(".row{flex-basis:50%;flex-basis:12px;order:0;}", &error), "same rule parses");
    check(!within.resolveLayout(node).flexBasisPercent && within.resolveLayout(node).flexOrder == 0 &&
          within.resolve(node).flexBasis == 12, "same-rule declaration ordering retains explicit zero");
    check(within.addRulesFromCss(".row{flex-basis:80%;}.row{flex-basis:16px;order:1;}", &error),
          "pixel basis before another extension parses");
    check(!within.resolveLayout(node).flexBasisPercent && within.resolve(node).flexBasis == 16,
          "an unrelated extension does not conceal the later pixel basis reset");
    const auto version = within.version();
    check(!within.addRulesFromCss(".row{order:1.5;}", &error) && within.version() == version,
          "invalid extension preserves transactional stylesheet parsing");
    StyleSheet switching;
    check(switching.addRulesFromCss(":root{--basis:12px;}.row{flex-basis:var(--basis);}", &error),
          "initial pixel variable parses");
    switching.setCustomProperty("--basis", "50%");
    check(switching.resolveLayout(node).flexBasisPercent == 50,
          "initial pixel variable can become a percentage without another extension");
    switching.setCustomProperty("--basis", "18px");
    check(!switching.resolveLayout(node).flexBasisPercent && switching.resolve(node).flexBasis == 18,
          "dynamic percentage can return to pixels");
    switching.setCustomProperty("--basis", "invalid");
    check(!switching.resolveLayout(node).flexBasisPercent && switching.resolve(node).flexBasis == 12,
          "invalid dynamic basis restores the initially parsed pixels");
    check(switching.addRulesFromCss(".row{max-width:120px;max-height:var(--height, 80px);}", &error),
          "maximum dimensions parse in independent layout metadata");
    check(switching.resolveLayout(node).maxWidth == 120 && switching.resolveLayout(node).maxHeight == 80,
          "maximum dimensions survive the restored old StyleBox layout");
    StyleSheet plain;
    check(plain.addRulesFromCss(".row{flex-basis:18px;color:#123456;}", &error), "plain CSS parses");
    check(!style_detail::ruleLayout(plain.rules().front()) && !plain.resolveLayout(node).flexBasisPercent &&
          plain.resolve(node).foreground->r == 0x12, "ordinary styles allocate no extension owner");
}

void storage() {
    static_assert(std::is_aggregate_v<StackFlex> && std::is_aggregate_v<StyleBox>);
    static_assert(int(ui::schema::Kind::DataMenu)==75 && int(ui::schema::Kind::Popup)==76 &&
                  int(ui::schema::Kind::MountPoint)==77 && int(ui::schema::Kind::StateView)==78 &&
                  int(ui::schema::Kind::Menu)==79 && int(ui::schema::Kind::VirtualTemplateList)==80 &&
                  int(ui::schema::Kind::TreeView)==81 && int(ui::schema::Kind::Table)==82);
    static_assert(!std::is_copy_constructible_v<Widget> && !std::is_move_constructible_v<Widget>);
    for (const auto engine : {StackEngine::Legacy, StackEngine::Yoga}) {
        auto stack = std::make_shared<Stack>(StackDirection::Row);
        stack->setEngine(engine); stack->setFrame({0, 0, 400, 100});
        auto a = std::make_shared<Label>(L"A"), b = std::make_shared<Label>(L"B");
        stack->add(a); stack->add(b);
        stack->setFlex(a, {0, 0, 10}); stack->setFlex(b, {0, 0, 20});
        stack->setFlexPresentation(a, {50, 2}); stack->setFlexPresentation(b, {{}, 1});
        oneui::test_support::RecordingCanvas canvas;
        stack->paint(canvas);
        check(a->frame().width == 200 && b->frame().x < a->frame().x,
              "private state drives percentage/order on both layout engines");
        auto sharedHandle = a;
        check(stack->flexPresentation(sharedHandle).basisPercent == 50,
              "copied shared child handles address the same presentation state");
        check(stack->remove(a), "remove child"); stack->add(a);
        check(!stack->flexPresentation(a).basisPercent && stack->flexPresentation(a).order == 0,
              "remove and re-add does not reuse obsolete presentation state");
        stack->setFlexPresentation(a, {25, 3}); stack->clearFlex(a);
        check(!stack->flexPresentation(a).basisPercent, "clearFlex clears all associated presentation");
        auto ephemeral = std::make_shared<Label>(L"weak");
        std::weak_ptr<Widget> weak = ephemeral;
        stack->setFlexPresentation(ephemeral, {10, 4}); ephemeral.reset();
        check(weak.expired(), "presentation does not retain a child widget");
        stack->setFlexPresentation(a, {10, 4});
        check(internal::WidgetPresentationAccess::find(*stack)->flex.size() == 2,
              "next update prunes expired child metadata");
    }
    alignas(Table) unsigned char memory[sizeof(Table)];
    for (int i = 0; i < 20; ++i) {
        auto* table = new (memory) Table;
        table->setColumns({{L"A", 100}, {L"B", 200}});
        check(!internal::WidgetPresentationAccess::find(*table)->table->widths,
              "reused object address starts with clean layout state");
        table->setColumnLayout(std::vector<float>{0, 80}, 32);
        check(internal::WidgetPresentationAccess::find(*table)->table->widths == std::vector<float>({0, 80}),
              "Table private state accepts column layout");
        table->setColumns({{L"C", 120}, {L"D", 240}});
        table->setColumnLayout({}, {});
        const auto& authored = internal::WidgetPresentationAccess::find(*table)->table->authored;
        check(authored.size() == 2 && authored[0].width == 120 && authored[1].width == 240,
              "clearing layout retains latest authored widths");
        table->~Table();
    }
    auto header=std::make_shared<PaneHeader>();
    header->setFrame({0,0,500,30});header->setActionLabel(PaneHeaderAction::Menu,L"Custom menu");
    oneui::test_support::RecordingCanvas headerCanvas;header->paint(headerCanvas);
    check(header->children().size()==7,"seventh PaneHeader action remains present with old object layout");
    const auto more=internal::WidgetPresentationAccess::find(*header)->paneHeaderMore;
    check(more&&more->frame().width==24,"private seventh action participates in layout");
    auto headerLife=header->lifetimeToken();header.reset();
    check(headerLife.expired(),"a retained seventh button does not keep its header alive");
}
} // namespace
int main() {
    try { cascade(); storage(); std::cout << "PASS presentation storage: cascade, query/copy/move/token fallback, lifecycle and both engines\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
