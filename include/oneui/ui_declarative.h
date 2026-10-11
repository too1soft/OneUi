#pragma once
#include "oneui/localization.h"
#include "oneui/controls/font_picker.h"
#include "oneui/controls/media_picker.h"
#include "oneui/controls/card.h"
#include "oneui/layout/theme_scope.h"
#include "oneui/controls/pane_header.h"
#include "oneui/controls/drag_handle.h"
#include "oneui/controls/reveal.h"
#include "oneui/ui_native_host.h"
#include "oneui/ui_native_theme.h"
#include "oneui/tabs_style_adapter.h"
#include "oneui/ui_keyed_tabs.h"
#include "oneui/ui_keyed_table.h"
#include "oneui/controls/icon_view.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/layout/adaptive_panes.h"
#include "oneui/layout/container_page.h"
#include "oneui/layout/container_query.h"
#include "oneui/ui.h"
#include "oneui/ui_density.h"
#include "oneui/ui_reactive.h"
#include "oneui/ui_template_support.h"
#include "oneui/style_adapter.h"
#include "oneui/ui_style_diagnostics.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/select.h"
#include "oneui/controls/state_view.h"
#include "oneui/controls/menu.h"
#include "oneui/controls/table.h"
#include "oneui/clipboard.h"
#include <codecvt>
#include <locale>
#include <set>

namespace oneui::ui {
inline std::wstring wide(const std::string& s) { return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(s); }
struct Element : Node {
    std::vector<std::shared_ptr<Widget>> collapsible;
    std::string component, classes;
    std::shared_ptr<Label> title, hint, error;
    std::shared_ptr<Widget> fieldControl;
    std::shared_ptr<KeyedTable> table;
    std::shared_ptr<KeyedTabs> tabs;
    struct SplitLimits {float first=80,second=80;};
    std::shared_ptr<SplitLimits> splitLimits;
    std::shared_ptr<Stack> content;
    Element(Node node, std::string type) : Node(std::move(node)), component(std::move(type)) {}
};

class StyleRegistry {
public:
    // Resolve authored CSS dimensions into the parent's Yoga constraints after
    // assembly. Native leaf measurement must not override explicit CSS sizes.
    void applyBaseLayouts();
    struct Source { std::string component, file; int line=0; };
private:
    struct Entry { std::weak_ptr<Widget> widget; std::string component, classes; std::weak_ptr<Stack> content; std::string tone, file; int line=0; bool invalid=false; std::string appearance,variant; };
    std::vector<Entry> entries_;
    StyleSheet sheet_;
    Density density_=Density::Comfortable;
    struct QueryState {std::vector<bool> matches;std::size_t version=~size_t{},treeVersion=0;Size measured{-1,-1};};
    struct LayoutDefaults {Size size;StackDirection direction=StackDirection::Column;bool wrap=false;std::optional<StackFlex> flex;std::weak_ptr<Stack> parent;std::weak_ptr<Widget> widget;};
    size_t treeVersion_=0;
    std::map<const Widget*,QueryState> queries_;
    std::map<const Widget*,LayoutDefaults> layoutDefaults_;
    std::shared_ptr<int> queryLifetime_=std::make_shared<int>(0);
    std::vector<std::string> queryDiagnostics_;
    std::map<const Widget*,std::shared_ptr<StyleSheet>> localThemes_;
    std::map<const ThemeScope*,std::shared_ptr<StyleSheet>> themeParents_;
    void applyLocalTheme(const std::shared_ptr<ThemeScope>&,const StyleSheet&);
    void refreshLocalThemes();
    void configureQuery(const std::shared_ptr<Widget>&,const StyleBox&);
    bool evaluateQuery(const std::shared_ptr<Stack>&);
    void applyLayout(const Entry&,const StyleBox&,const std::shared_ptr<Stack>&);
    void restoreQueryLayouts();
    void apply(const Entry& entry, const StyleSheet& supplied) {
        auto w = entry.widget.lock(); if (!w) return;
        std::optional<StyleSheet> filtered;
        const StyleSheet* effective=&supplied;
        if(auto local=localThemes_.find(w.get());local!=localThemes_.end()) {
            if(&supplied==&sheet_)effective=local->second.get();
            else {filtered=supplied;for(auto token:semanticColorTokens)if(auto v=local->second->customProperty(std::string("--")+token))filtered->setCustomProperty(std::string("--")+token,*v);effective=&*filtered;}
        }
        const auto& sheet=*effective;
        StyleNode n; n.tag = syntax::components().at(entry.component); n.classes.push_back("one-" + entry.component);
        std::istringstream tokens(entry.classes); for (std::string c; tokens >> c;) n.classes.push_back(c);
        if(!entry.variant.empty())n.classes.push_back(entry.variant);
        if(!entry.appearance.empty())n.classes.push_back("surface-"+entry.appearance);
        if(!entry.tone.empty()) n.classes.push_back("tone-"+entry.tone);
        if(entry.invalid) n.classes.push_back("invalid");
        if(entry.component=="Input" || entry.component=="SearchInput" || entry.component=="Select") {
            const auto size=w->preferredSize();w->setPreferredSize({size.width,controlHeight(density_)});
        }
        if(auto table=std::dynamic_pointer_cast<Table>(w)) {
            // Retain the top business row when changing density.
            const float row=table->rowHeight()>0?table->scrollOffset()/table->rowHeight():0;
            table->setRowHeight(tableRowHeight(density_));table->setHeaderHeight(controlHeight(density_));
            table->setScrollOffset(row*tableRowHeight(density_));
        }
        auto box = sheet.resolve(n);
        rememberStyleDiagnostic(w,n,box);
        configureQuery(w,box);
        if(auto page=std::dynamic_pointer_cast<ContainerPage>(w))page->setDisplay(box.display);
        else if(box.display)w->setLayoutVisible(*box.display);
        if(box.width||box.height) {auto size=w->preferredSize();w->setPreferredSize({box.width.value_or(size.width),box.height.value_or(size.height)});}
        if(entry.component=="PaneGrid")w->setStyleScope(std::make_shared<StyleSheet>(sheet));
        if(auto header=std::dynamic_pointer_cast<PaneHeader>(w)) {
            auto palette=nativePalette(sheet);
            if(!box.foreground)box.foreground=palette.ink;
            if(!box.background.color)box.background.color=palette.surface;
            if(!box.borderColor)box.borderColor=palette.accent;
            header->setStyleBox(box);header->setStyleScope(std::make_shared<StyleSheet>(sheet));return;
        }
        if(auto host=std::dynamic_pointer_cast<NativeHostView>(w)) {
            if(host->themed() && !host->children().empty())applyNativeTheme(*host->children().front(),sheet,host->hasCompositionOwner());return;
        }
        if(auto card=std::dynamic_pointer_cast<Card>(w)){card->setStyleBox(box);return;}
        if(auto state=std::dynamic_pointer_cast<StateView>(w)){state->setStyleSheet(std::make_shared<StyleSheet>(sheet),n);return;}
        if(auto menu=std::dynamic_pointer_cast<Menu>(w)){menu->setStyleSheet(std::make_shared<StyleSheet>(sheet),n);return;}
        if(auto surface=std::dynamic_pointer_cast<InteractiveSurface>(w)) {
            surface->setStyle(interactiveSurfaceStyleFromStyleSheet(sheet,n));
            surface->setPadding(box.padding.value_or(Insets{}));return;
        }
        if(auto slider=std::dynamic_pointer_cast<Slider>(w)){applyNativeTheme(*slider,sheet);return;}
        if(std::dynamic_pointer_cast<FontPicker>(w)||std::dynamic_pointer_cast<MediaPicker>(w)||std::dynamic_pointer_cast<MediaPreview>(w)){w->setStyleScope(std::make_shared<StyleSheet>(sheet));return;}
        if(auto icon=std::dynamic_pointer_cast<IconView>(w)) {icon->setColor(box.foreground.value_or(nativePalette(sheet).ink));return;}
        if(auto tabs=std::dynamic_pointer_cast<Tabs>(w)) {
            bool authored=false;
            for(auto state:{StyleStateNone,StyleStateHover,StyleStateActive,StyleStateSelected,StyleStateDisabled,StyleStateFocus}) {
                auto node=n;node.state=state;const auto value=sheet.resolve(node);
                authored|=value.background.color.has_value() || value.foreground.has_value() || value.borderColor.has_value() ||
                    value.borderWidth.has_value() || value.radius.has_value() || value.content.backgroundColor.has_value() ||
                    value.content.radius.has_value() || value.padding.has_value() || value.fontSize.has_value() ||
                    value.fontWeight.has_value() || value.outlineColor.has_value() || value.outlineWidth.has_value() || value.outlineOffset.has_value();
            }
            if(authored)tabs->setStyleOverride(tabsStyleOverrideFromStyleSheet(sheet,n));
            else applyNativeTheme(*tabs,sheet);
            return;
        }
        if(std::dynamic_pointer_cast<SplitView>(w) || std::dynamic_pointer_cast<ProgressBar>(w)) {applyNativeTheme(*w,sheet);return;}
        if(auto chart=std::dynamic_pointer_cast<TimeSeriesChart>(w)) {applyNativeTheme(*w,sheet);if(box.foreground || box.borderColor || box.fontSize || box.gap)chart->setStyleBox(box);return;}
        if (auto v = std::dynamic_pointer_cast<Label>(w)) {
            v->setColor(box.foreground.value_or(Color{30,40,35,255})); v->setFontSize(box.fontSize.value_or(14)); v->setFontWeight(box.fontWeight.value_or(400));
        } else if (auto v = std::dynamic_pointer_cast<Stack>(w)) {
            v->setStyleBox(box); v->setGap(box.gap.value_or(0)); v->setPadding(box.padding.value_or(Insets{}));
            if(box.flexDirection)v->setDirection(*box.flexDirection=="row"?StackDirection::Row:StackDirection::Column);
            if(box.flexWrap&&!v->setWrap(*box.flexWrap))throw std::invalid_argument("CSS flex-wrap requires Yoga");
            // Content's horizontal shell owns decoration/padding, while gap
            // belongs to its vertical body. Neither changes the authored tree.
            if (auto body = entry.content.lock()) { v->setGap(0); body->setGap(box.gap.value_or(0)); }
        } else if (auto v = std::dynamic_pointer_cast<Button>(w)) v->setStyleOverride(buttonStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<TextField>(w)) {
            v->setStyleOverride(textFieldStyleOverrideFromStyleSheet(sheet,n));
            v->setFontSize(box.fontSize.value_or(14));
        }
        else if (auto v = std::dynamic_pointer_cast<Switch>(w)) v->setStyleOverride(switchStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<Select>(w)) v->setStyleOverride(selectStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<List>(w)) v->setStyleOverride(listStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<VirtualList>(w)) v->setStyleOverride(listStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<Table>(w)) v->setStyleOverride(tableStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<ScrollView>(w)) {
            v->setStyleBox(box);
            v->setScrollbarStyle(box.scrollbarColor.value_or(box.foreground.value_or(Color{148,163,184,180})),
                                 box.scrollbarWidth.value_or(4.0f));
        }
        else if (auto v = std::dynamic_pointer_cast<AdaptivePanes>(w)) v->setStyleBox(box);
        else if (auto v = std::dynamic_pointer_cast<Reveal>(w)) v->setTransition({box.transitionDurationMs.value_or(220),box.transitionEasing.value_or(EasingCurve::EaseOutCubic)});
    }
public:
    const std::vector<std::string>& queryDiagnostics() const{return queryDiagnostics_;}
    ONEUI_API void inheritTransient(const std::shared_ptr<StyleSheet>& sheet);
    void variant(const Element& e,const std::wstring& value) {
        if((e.component!="Button" && e.component!="ToolButton") || (value!=L"normal" && value!=L"primary" && value!=L"danger" && value!=L"ghost"))throw std::invalid_argument("Button variant must be normal, primary or danger");
        for(auto& entry:entries_)if(entry.widget.lock()==e.widget){entry.variant=std::string(value.begin(),value.end());apply(entry,sheet_);return;}
    }
    void appearance(const Element& e,const std::wstring& value) {
        if(e.component!="Surface" || (value!=L"flat" && value!=L"outlined" && value!=L"raised" && value!=L"tinted"))throw std::invalid_argument("Surface appearance must be flat, outlined, raised or tinted");
        for(auto& entry:entries_)if(entry.widget.lock()==e.widget){entry.appearance=std::string(value.begin(),value.end());apply(entry,sheet_);return;}
    }
    void invalid(const std::shared_ptr<Widget>& widget,bool value) {
        for(auto& entry:entries_)if(entry.widget.lock()==widget) {
            if(entry.invalid!=value){entry.invalid=value;apply(entry,sheet_);}return;
        }
    }
    void locate(const Element& e,std::string file,int line) {
        e.widget->setDiagnosticSource({e.component,file,line});
        for(auto& entry:entries_) if(entry.widget.lock()==e.widget) {entry.file=std::move(file);entry.line=line;return;}
    }
    std::map<const Widget*,Source> sources() const {
        std::map<const Widget*,Source> result;
        for(auto& entry:entries_) if(auto w=entry.widget.lock()) result.emplace(w.get(),Source{entry.component,entry.file,entry.line});
        return result;
    }
    void tone(const Element& e,const std::wstring& value) {
        if(value!=L"neutral" && value!=L"success" && value!=L"warning" && value!=L"error" && value!=L"pending") throw std::invalid_argument("Status tone must be neutral, success, warning, error or pending");
        for(auto& entry:entries_) if(entry.widget.lock()==e.widget) {
            entry.tone.clear();for(wchar_t c:value)entry.tone.push_back(static_cast<char>(c));
            apply(entry,sheet_);return;
        }
    }
    void add(const Element& e) {
        ++treeVersion_;
        if(e.widget->diagnosticSource().file.empty())e.widget->setDiagnosticSource({e.component,{},0});
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[](auto& entry){return entry.widget.expired();}),entries_.end());
        for (auto& entry : entries_) if (entry.widget.lock() == e.widget) { entry.component=e.component;entry.classes = e.classes; apply(entry,sheet_); return; }
        localThemes_.erase(e.widget.get()); // A new widget may reuse a retired address.
        entries_.push_back({e.widget,e.component,e.classes,e.content}); apply(entries_.back(),sheet_);
        if(auto scope=std::dynamic_pointer_cast<ThemeScope>(e.widget)){refreshLocalThemes();}
    }
    void replace(const std::string& validatedCss,Density density=Density::Comfortable) {
        StyleSheet next; std::string error;
        if (!next.addRulesFromCss(validatedCss,&error)) throw std::invalid_argument(error);
        // Validate resolved tokens too: var(--gradient) must not bypass the
        // strict entry's component consumption checks.
        for(const auto& rule:next.rules()) if(rule.box.background.gradient || rule.box.background.gradientStart) {
            const auto tag=rule.selector.substr(0,rule.selector.find_first_of(".:"));
            if(tag!="stack" && tag!="scroll-view" && tag!="button" && tag!="input" && tag!="card")
                throw std::invalid_argument("Gradient is not consumed by "+tag+" in "+rule.selector);
        }
        // Force variable resolution before committing the sheet.
        for (auto& entry : entries_) { StyleNode n{syntax::components().at(entry.component), {"one-"+entry.component},0}; next.resolve(n); }
        next.setTransientStyleSheet(sheet_.transientStyleSheet());
        sheet_ = std::move(next);density_=density;queries_.clear();localThemes_.clear();themeParents_.clear();
        restoreQueryLayouts();
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[](auto& e) { return e.widget.expired(); }),entries_.end());
        applyAll();
    }
    std::size_t size() const { return std::count_if(entries_.begin(),entries_.end(),[](const auto& e){return !e.widget.expired();}); }
    void appendScoped(const std::string& css) {
        auto next=sheet_;std::string error;
        if(!next.addRulesFromCss(css,&error))throw std::invalid_argument(error);
        sheet_=std::move(next);queries_.clear();
        for(auto& entry:entries_)apply(entry,sheet_);
    }
    void remapWidgets(const std::map<const Widget*,std::shared_ptr<Widget>>& replacements) {
        queries_.clear();layoutDefaults_.clear();++treeVersion_;
        for(auto& entry:entries_) {
            if(auto widget=entry.widget.lock()) {
                auto found=replacements.find(widget.get());
                if(found!=replacements.end())entry.widget=found->second;
            }
            if(auto content=entry.content.lock()) {
                auto found=replacements.find(content.get());
                if(found!=replacements.end())entry.content=std::dynamic_pointer_cast<Stack>(found->second);
            }
        }
    }
    void applyAll() { localThemes_.clear();themeParents_.clear();for(auto& entry:entries_)apply(entry,sheet_);refreshLocalThemes();applyBaseLayouts(); }
};

// Implemented by the desktop host, evaluated only by legacy callers that omit
// the service argument. Platform-free runtime callers pass their own service.
ONEUI_API std::shared_ptr<Clipboard> defaultMountClipboard();

class Mount {
    struct Life { bool alive=true; std::map<std::size_t,std::function<void()>> pending; std::size_t next = 0; };
    std::shared_ptr<Life> life_ = std::make_shared<Life>();
    std::vector<Subscription> subscriptions_;
    std::vector<std::shared_ptr<void>> owned_;
    std::function<void()> schedule_;
    std::shared_ptr<StyleRegistry> styles_;
    std::string scope_;
    std::map<std::string,std::weak_ptr<Widget>> references_;
    struct NativeEntry { std::shared_ptr<Widget> widget; std::shared_ptr<void> owner; };
    using NativeEntries = std::map<std::string, NativeEntry>;
    std::shared_ptr<NativeEntries> natives_ = std::make_shared<NativeEntries>();
    std::shared_ptr<Clipboard> clipboard_;
public:
    explicit Mount(std::function<void()> schedule = {}, std::shared_ptr<StyleRegistry> styles = std::make_shared<StyleRegistry>(),
                   std::shared_ptr<Clipboard> clipboard = defaultMountClipboard())
        : schedule_(std::move(schedule)), styles_(std::move(styles)), clipboard_(std::move(clipboard)) {}
    Mount(const Mount&) = delete;
    ~Mount() { life_->alive=false;life_.reset(); subscriptions_.clear(); owned_.clear(); }
    void setScope(std::string scope) { scope_ = std::move(scope); }
    std::string scope() const { return scope_; }
    auto styles() const { return styles_; }
    auto clipboard() const { return clipboard_; }
    auto scheduler() const { return schedule_; }
    void attachNative(const std::string& name,const std::shared_ptr<NativeHostView>& host) {
        const auto found=natives_->find(name);
        if(found!=natives_->end())host->attach(found->second.widget,found->second.owner);
    }
    struct Diagnostics { std::size_t subscriptions, ownedObjects, pendingUpdates; };
    Diagnostics diagnostics() const { return {subscriptions_.size(),owned_.size(),life_->pending.size()}; }
    void remember(const std::string& name,const std::shared_ptr<Widget>& widget) {
        if(name.empty() || !widget)throw std::invalid_argument("ref requires a name and widget");
        auto found=references_.find(name);
        if(found!=references_.end() && !found->second.expired())throw std::invalid_argument("Duplicate ref: "+name);
        references_[name]=widget;
    }
    std::shared_ptr<Widget> find(const std::string& name) const {
        auto found=references_.find(name);
        auto widget=found==references_.end()?nullptr:found->second.lock();
        if(!widget)throw std::invalid_argument("Missing ref: "+name);
        return widget;
    }
    // Register once before building; no implicit replacement or reparenting.
    void registerNative(std::string name, std::shared_ptr<Widget> widget, std::shared_ptr<void> owner = {}) {
        if(name.empty() || !widget) throw std::invalid_argument("NativeHost registration requires a name and widget");
        if(!natives_->emplace(std::move(name),NativeEntry{std::move(widget),std::move(owner)}).second)
            throw std::invalid_argument("Duplicate NativeHost registration");
    }
    Element nativeHost(const std::string& name) {
        const auto found=natives_->find(name);
        if(found==natives_->end())throw std::invalid_argument("Missing NativeHost registration: "+name);
        Element result(Node(std::make_shared<NativeHostView>(found->second.widget,found->second.owner)),"NativeHost");
        result.classes=scope_; styles_->add(result); return result;
    }
    void locate(const Element& e,std::string file,int line) { styles_->locate(e,std::move(file),line); }
    void flush() {
        auto life = life_;
        while (life->alive && !life->pending.empty()) { auto jobs = std::move(life->pending); life->pending.clear(); for (auto& job : jobs) {if(!life->alive)return;job.second();} }
        if(!life->alive)return;
        auto children=childFlush_;
        for (auto& fn : children) {if(!life->alive)return;fn();}
    }
    std::vector<std::function<void()>> childFlush_;
    template<class T> void own(std::shared_ptr<T> object) { owned_.push_back(std::move(object)); }
    // Runtime Elements retain active adapters; retire adapters whose node vanished.
    void collectUnusedOwned() {
        owned_.erase(std::remove_if(owned_.begin(),owned_.end(),
            [](const auto& object){return object.use_count()==1;}),owned_.end());
    }
    template<class Source, class F> void watch(Source& source, F apply) {
        apply(source.get());
        const auto id = ++life_->next; auto weak = std::weak_ptr<Life>(life_); auto schedule = schedule_;
        subscriptions_.push_back(source.subscribeScoped([weak,id,apply,schedule](const auto& value) mutable {
            if (auto life = weak.lock()) {
                life->pending[id] = [apply,value]() mutable { apply(value); };
                detail::defer(life.get(),[weak,schedule] { if (!weak.expired() && schedule) schedule(); });
            }
        }));
    }
    ONEUI_API Element make(std::string type, std::vector<Element> children = {}, std::string classes = {});
    ONEUI_API void set(Element& e, const std::string& key, const std::wstring& value);
    ONEUI_API void set(Element& e,const std::string& key,const wchar_t* value);
    ONEUI_API void set(Element& e,const std::string& key,const std::string& value);
    ONEUI_API void set(Element& e,const std::string& key,bool value);
    ONEUI_API void set(Element& e,const std::string& key,float value);
    ONEUI_API void set(Element& e,const std::string& key,int value);
    ONEUI_API void set(Element& e,const std::string&,const std::vector<std::wstring>& items);
    ONEUI_API void set(Element& e,const std::string&,const std::vector<ListItem>& items);
    ONEUI_API void set(Element& e,const std::string&,const std::vector<TableColumn>& columns);
    ONEUI_API void set(Element& e,const std::string&,const std::vector<TableRow>& rows);
    ONEUI_API void set(Element& e,const std::string&,const std::vector<TabItem>& items);
    ONEUI_API void set(Element& e,const std::string&,const std::vector<TimeSeriesChartSeries>& series);
    template<class Source> void bind(Element e,const std::string& key,Source& source) {
        watch(source,[this,e,key](const auto& v) mutable { set(e,key,v); });
    }
    template<class T,class Source> void bindTyped(Element e,const std::string& key,Source& source) {
        static_assert(std::is_same_v<T,std::decay_t<decltype(source.get())>>, "Template property and ViewModel type do not match");
        bind(e,key,source);
    }
    void model(Element e,State<std::wstring>& source) {
        if(e.tabs) {
            auto binding=std::make_shared<Binding<std::wstring>>(source);own(binding);
            e.tabs->selectionValue=[binding]{const std::wstring fallback;return std::wstring(binding->get(fallback));};
            e.tabs->selected=[binding](std::wstring id){std::wstring fallback;binding->set(std::move(id),fallback);};
            bind(e,"selectedKey",source);
        } else if(e.table) {
            auto binding=std::make_shared<Binding<std::wstring>>(source); own(binding);
            e.table->selectionValue=[binding] {const std::wstring fallback; return std::wstring(binding->get(fallback));};
            bind(e,"selectedKey",source); e.table->selected=[binding](std::wstring key) { std::wstring fallback; binding->set(std::move(key),fallback); };
        } else { e.as<TextField>()->bindText(source); }
    }
    void model(Element e,State<float>& source) {
        auto binding=std::make_shared<Binding<float>>(source);own(binding);bind(e,"ratio",source);
        e.as<SplitView>()->setOnSplitRatioChanged([binding](float value){float fallback=0.5f;binding->set(value,fallback);});
    }
    void closeTab(Element e,std::function<void(std::wstring)> callback) {
        if(!e.tabs)throw std::invalid_argument("close requires Tabs");
        auto weak=std::weak_ptr<Life>(life_);e.tabs->closed=[weak,callback=std::move(callback)](std::wstring id){if(!weak.expired() && callback)callback(std::move(id));};
    }
    void model(Element e,State<bool>& source) { e.as<Switch>()->bindChecked(source); }
    void model(Element e,State<int>& source) { e.as<Select>()->bindSelectedIndex(source); }
    template<class T> void modelTyped(Element e,State<T>& source) { model(e,source); }
    void click(Element e,VmCommand& command) {
        auto weak=std::weak_ptr<Life>(life_);
        auto invoke=[weak,&command] { if(!weak.expired()) command.execute(); };
        if(auto surface=e.as<InteractiveSurface>())surface->setOnClick(invoke);
        else e.as<Button>()->setOnClick(invoke);
        auto enabled=std::make_shared<Computed<bool>>([&command] { return !command.canExecute.get() || command.running.get(); },command.canExecute,command.running);
        bind(e,"disabled",*enabled); own(enabled);
    }
    void tableEvent(Element e,const std::string& event,VmCommand& command) {
        auto table=e.as<Table>();if(!table)throw std::invalid_argument("Table events require DataTable");
        auto weak=std::weak_ptr<Life>(life_);
        if(event=="activate")table->setOnActivated([weak,&command](int){if(!weak.expired())command.execute();});
        else if(event=="delete")table->setOnDeleteRequested([weak,&command](const auto&){if(!weak.expired())command.execute();});
        else throw std::invalid_argument("Unknown DataTable event: "+event);
    }
    template<class Source> void condition(Element e,Source& condition,bool inverse=false) {
        watch(condition,[w=e.widget,inverse](bool visible) { w->setVisible(visible != inverse); });
    }
    void condition(Element e,bool visible,bool inverse=false) {e.widget->setVisible(visible!=inverse);}
    // Items are stable shared objects. The factory receives an owned item, never
    // a reference into a vector; removal destroys its subscriptions and scope.
    template<class Source,class Key,class Factory> Element repeat(Source& source,Key key,Factory factory) {
        using Items=std::decay_t<decltype(source.get())>; using Item=typename Items::value_type;
        struct Row { Item item; std::shared_ptr<Mount> mount; Element element; };
        auto rows=std::make_shared<std::map<std::wstring,Row>>();
        auto result=make("Column"); auto styles=styles_; auto schedule=schedule_; auto scope=scope_; auto natives=natives_; auto clipboard=clipboard_;
        watch(source,[rows,result,styles,schedule,scope,natives,clipboard,key,factory](const Items& items) mutable {
            std::set<std::wstring> keys; for(auto& item:items) if(!item || key(item).empty() || !keys.insert(key(item)).second) throw std::invalid_argument("v-for requires nonnull shared items and unique nonempty keys");
            std::map<std::wstring,Row> next; std::vector<std::shared_ptr<Widget>> widgets;
            for(auto& item:items) {
                auto id=key(item); auto found=rows->find(id);
                if(found!=rows->end()) {
                    if(found->second.item!=item) throw std::invalid_argument("A retained v-for key must retain its shared item identity; mutate the item's State fields");
                    next.emplace(id,std::move(found->second));
                } else { auto mount=std::make_shared<Mount>(schedule,styles,clipboard); mount->setScope(scope); mount->natives_=natives; auto element=factory(*mount,item); next.emplace(id,Row{item,mount,element}); }
                auto& row=next.at(id); widgets.push_back(row.element.widget); result.template as<Stack>()->setFlex(row.element.widget,row.element.flex);
            }
            result.template as<Stack>()->reconcileChildren(std::move(widgets)); *rows=std::move(next);
        });
        childFlush_.push_back([rows] { for(auto& row:*rows) row.second.mount->flush(); }); own(rows); return result;
    }
};
using Slots = std::map<std::string,std::function<std::vector<Element>(Mount&)>>;
} // namespace oneui::ui
#include "oneui/ui_container_styles.h"

#include "oneui/ui_local_theme_styles.h"
