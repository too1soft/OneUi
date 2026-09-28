#pragma once
#include "oneui/controls/tabs.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/controls/virtual_list.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/table.h"
#include "oneui/controls/time_series_chart.h"
#include "oneui/layout/split_view.h"
#include "oneui/style_adapter.h"
namespace oneui::ui {
struct NativePalette {
    Color surface{255,255,255},ink{28,43,35},muted{84,103,91},line{186,200,191},selection{213,233,215},accent{40,97,61},hover{213,226,215};
};
inline NativePalette nativePalette(const StyleSheet& sheet) {
    // Resolve through the parser, including custom token indirection. No second color parser.
    if(!sheet.customProperty("--surface"))return {};
    StyleSheet tokens;for(const auto& item:sheet.customProperties())tokens.setCustomProperty(item.first,item.second);
    std::string error;
    if(!tokens.addRulesFromCss("native { background-color:var(--surface); color:var(--ink); placeholder-color:var(--muted); border-color:var(--line); selection-color:var(--selection); caret-color:var(--accent); content-background-color:var(--hover); }",&error))return {};
    const auto box=tokens.resolve({"native",{},0});NativePalette p;
    p.surface=box.background.color.value_or(p.surface);p.ink=box.foreground.value_or(p.ink);p.muted=box.placeholderColor.value_or(p.muted);
    p.line=box.borderColor.value_or(p.line);p.selection=box.selectionColor.value_or(p.selection);p.accent=box.caretColor.value_or(p.accent);p.hover=box.content.backgroundColor.value_or(p.hover);return p;
}
inline void applyNativeTheme(Widget& widget,const StyleSheet& sheet,bool animate=true) {
    if(auto field=dynamic_cast<TextField*>(&widget)){auto style=textFieldStyleOverrideFromStyleSheet(sheet,{"input",{"one-Input"},0});
        if(!animate) {
            // A newly wrapped widget already has a forwarding scheduler, but no
            // window can tick it yet. Install initial colors without a transition.
            auto initial=style;
            for(auto* state:{&initial.normal,&initial.hovered,&initial.disabled,&initial.readOnly,&initial.focusVisible})
                if(*state)(*state)->transition=TransitionSpec{0};
            field->setStyleOverride(std::move(initial));
        }
        field->setStyleOverride(std::move(style));return;}
    if(auto table=dynamic_cast<Table*>(&widget)){table->setStyleOverride(tableStyleOverrideFromStyleSheet(sheet,{"table",{"one-DataTable"},0}));return;}
    const auto p=nativePalette(sheet);
    if(auto progress=dynamic_cast<ProgressBar*>(&widget)){ProgressBarStyleOverride style;style.trackBackground=p.line;style.fill=p.accent;style.radius=2;progress->setStyleOverride(style);return;}
    if(auto tabs=dynamic_cast<Tabs*>(&widget)) {
        TabsStateStyleOverride n;n.background=p.surface;n.border=p.line;n.borderWidth=0;n.radius=0;n.itemRadius=5;
        n.itemBackground=Color{0,0,0,0};n.itemForeground=p.muted;n.selectedItemBackground=p.selection;n.selectedItemForeground=p.ink;n.selectedItemBorder=p.accent;
        FocusRingStyleOverride focus;focus.color=p.accent;focus.width=1;n.focusRing=focus;
        n.fontSize=13;n.fontWeight=500;TabsStyleOverride style;style.normal=n;
        auto hover=n;hover.itemBackground=p.hover;style.hovered=hover;style.pressed=hover;tabs->setStyleOverride(style);
    } else if(auto list=dynamic_cast<VirtualList*>(&widget)) {
        ListStateStyleOverride n;n.background=p.surface;n.borderWidth=0;n.radius=0;n.rowRadius=6;n.separator=Color{0,0,0,0};
        n.titleColor=p.ink;n.detailColor=p.muted;n.selectedRowBackground=p.selection;n.selectedTitleColor=p.ink;n.selectedDetailColor=p.muted;
        n.rowInset=Insets{3,6};n.titleFontSize=13;n.detailFontSize=11;n.titleFontWeight=500;n.titleOffsetY=10;n.detailOffsetY=30;n.scrollbarColor=p.line;n.scrollbarWidth=3;
        ListStyleOverride style;style.normal=n;auto hover=n;hover.rowBackground=p.hover;style.hovered=hover;list->setStyleOverride(style);
    } else if(auto chart=dynamic_cast<TimeSeriesChart*>(&widget)) {
        StyleBox box;box.borderColor=Color{p.line.r,p.line.g,p.line.b,90};box.foreground=p.muted;box.fontSize=10;chart->setStyleBox(box);
    } else if(auto split=dynamic_cast<SplitView*>(&widget))split->setDividerColors(p.line,p.accent);
}
}
