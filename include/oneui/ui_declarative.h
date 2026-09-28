#pragma once
#include "oneui/controls/reveal.h"
#include "oneui/ui_native_host.h"
#include "oneui/ui_native_theme.h"
#include "oneui/ui_keyed_tabs.h"
#include "oneui/controls/icon_view.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/layout/adaptive_panes.h"
#include "oneui/ui.h"
#include "oneui/ui_density.h"
#include "oneui/ui_reactive.h"
#include "oneui/ui_template_support.h"
#include "oneui/style_adapter.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/select.h"
#include "oneui/controls/table.h"
#include "oneui/platform/window.h"
#include <codecvt>
#include <locale>
#include <set>

namespace oneui::ui {
inline std::wstring wide(const std::string& s) { return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(s); }
struct TableRow {
    std::wstring id;
    std::vector<std::wstring> cells;
    bool operator==(const TableRow& r) const { return id == r.id && cells == r.cells; }
};

class KeyedTable {
    std::shared_ptr<Table> table_;
    std::vector<TableRow> rows_;
    bool updating_ = false;
public:
    std::function<void(std::wstring)> selected;
    std::function<std::wstring()> selectionValue;
    explicit KeyedTable(std::shared_ptr<Table> table) : table_(std::move(table)) {
        table_->setOnChanged([this](int i) { if (!updating_ && selected) selected(i >= 0 && i < int(rows_.size()) ? rows_[i].id : L""); });
    }
    ~KeyedTable() { table_->setOnChanged({}); }
    void select(const std::wstring& id) {
        const auto found = std::find_if(rows_.begin(), rows_.end(), [&](auto& row) { return row.id == id; });
        updating_ = true; table_->setSelectedIndex(found == rows_.end() ? -1 : int(found - rows_.begin())); updating_ = false;
    }
    void update(const std::vector<TableRow>& rows) {
        std::set<std::wstring> keys;
        for (auto& r : rows) if (r.id.empty() || !keys.insert(r.id).second) throw std::invalid_argument("DataTable requires unique nonempty row IDs");
        if (rows == rows_) return;
        const auto offset = table_->scrollOffset();
        std::vector<std::wstring> selectedKeys;
        for (int i : table_->selectedIndices()) if (i >= 0 && i < int(rows_.size())) selectedKeys.push_back(rows_[i].id);
        bool sameKeys = rows.size() == rows_.size();
        if (sameKeys) for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].id != rows_[i].id) { sameKeys = false; break; }
        updating_ = true;
        if (sameKeys) {
            for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].cells != rows_[i].cells) table_->updateRow(i, rows[i].cells);
        } else {
            std::vector<std::vector<std::wstring>> cells; cells.reserve(rows.size());
            for (auto& r : rows) cells.push_back(r.cells);
            table_->setRows(std::move(cells));
        }
        rows_ = rows;
        if (!sameKeys) {
            std::vector<int> indices;
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (std::find(selectedKeys.begin(), selectedKeys.end(), rows[i].id) != selectedKeys.end()) indices.push_back(int(i));
            table_->setSelectedIndices(std::move(indices)); table_->setScrollOffset(offset);
        }
        if (selectionValue) { select(selectionValue()); table_->setScrollOffset(offset); }
        updating_ = false;
        if (selected) { const int i = table_->selectedIndex(); selected(i >= 0 && i < int(rows.size()) ? rows[i].id : L""); }
    }
};

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
    struct Source { std::string component, file; int line=0; };
private:
    struct Entry { std::weak_ptr<Widget> widget; std::string component, classes; std::weak_ptr<Stack> content; std::string tone, file; int line=0; bool invalid=false; std::string appearance,variant; };
    std::vector<Entry> entries_;
    StyleSheet sheet_;
    Density density_=Density::Comfortable;
    void apply(const Entry& entry, const StyleSheet& sheet) {
        auto w = entry.widget.lock(); if (!w) return;
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
        if(auto host=std::dynamic_pointer_cast<NativeHostView>(w)) {
            if(host->themed())applyNativeTheme(*host->children().front(),sheet,host->hasCompositionOwner());return;
        }
        if(auto icon=std::dynamic_pointer_cast<IconView>(w)) {icon->setColor(box.foreground.value_or(nativePalette(sheet).ink));return;}
        if(std::dynamic_pointer_cast<Tabs>(w) || std::dynamic_pointer_cast<SplitView>(w) || std::dynamic_pointer_cast<ProgressBar>(w)) {applyNativeTheme(*w,sheet);return;}
        if(auto chart=std::dynamic_pointer_cast<TimeSeriesChart>(w)) {applyNativeTheme(*w,sheet);if(box.foreground || box.borderColor || box.fontSize || box.gap)chart->setStyleBox(box);return;}
        if (auto v = std::dynamic_pointer_cast<Label>(w)) {
            v->setColor(box.foreground.value_or(Color{30,40,35,255})); v->setFontSize(box.fontSize.value_or(14)); v->setFontWeight(box.fontWeight.value_or(400));
        } else if (auto v = std::dynamic_pointer_cast<Stack>(w)) {
            v->setStyleBox(box); v->setGap(box.gap.value_or(0)); v->setPadding(box.padding.value_or(Insets{}));
            // Content's horizontal shell owns decoration/padding, while gap
            // belongs to its vertical body. Neither changes the authored tree.
            if (auto body = entry.content.lock()) { v->setGap(0); body->setGap(box.gap.value_or(0)); }
        } else if (auto v = std::dynamic_pointer_cast<Button>(w)) v->setStyleOverride(buttonStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<TextField>(w)) v->setStyleOverride(textFieldStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<Switch>(w)) v->setStyleOverride(switchStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<Select>(w)) v->setStyleOverride(selectStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<Table>(w)) v->setStyleOverride(tableStyleOverrideFromStyleSheet(sheet,n));
        else if (auto v = std::dynamic_pointer_cast<ScrollView>(w)) v->setStyleBox(box);
        else if (auto v = std::dynamic_pointer_cast<AdaptivePanes>(w)) v->setStyleBox(box);
        else if (auto v = std::dynamic_pointer_cast<Reveal>(w)) v->setTransition({box.transitionDurationMs.value_or(220),box.transitionEasing.value_or(EasingCurve::EaseOutCubic)});
    }
public:
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
        if(e.widget->diagnosticSource().file.empty())e.widget->setDiagnosticSource({e.component,{},0});
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[](auto& entry){return entry.widget.expired();}),entries_.end());
        for (auto& entry : entries_) if (entry.widget.lock() == e.widget) { entry.component=e.component;entry.classes = e.classes; apply(entry,sheet_); return; }
        entries_.push_back({e.widget,e.component,e.classes,e.content}); apply(entries_.back(),sheet_);
    }
    void replace(const std::string& validatedCss,Density density=Density::Comfortable) {
        StyleSheet next; std::string error;
        if (!next.addRulesFromCss(validatedCss,&error)) throw std::invalid_argument(error);
        // Validate resolved tokens too: var(--gradient) must not bypass the
        // strict entry's component consumption checks.
        for(const auto& rule:next.rules()) if(rule.box.background.gradient || rule.box.background.gradientStart) {
            const auto tag=rule.selector.substr(0,rule.selector.find_first_of(".:"));
            if(tag!="stack" && tag!="scroll-view" && tag!="button" && tag!="input")
                throw std::invalid_argument("Gradient is not consumed by "+tag+" in "+rule.selector);
        }
        // Force variable resolution before committing the sheet.
        for (auto& entry : entries_) { StyleNode n{syntax::components().at(entry.component), {"one-"+entry.component},0}; next.resolve(n); }
        sheet_ = std::move(next);density_=density;
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[](auto& e) { return e.widget.expired(); }),entries_.end());
        for (auto& entry : entries_) apply(entry,sheet_);
    }
    std::size_t size() const { return std::count_if(entries_.begin(),entries_.end(),[](const auto& e){return !e.widget.expired();}); }
};

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
public:
    explicit Mount(std::function<void()> schedule = {}, std::shared_ptr<StyleRegistry> styles = std::make_shared<StyleRegistry>())
        : schedule_(std::move(schedule)), styles_(std::move(styles)) {}
    Mount(const Mount&) = delete;
    ~Mount() { life_->alive=false;life_.reset(); subscriptions_.clear(); owned_.clear(); }
    void setScope(std::string scope) { scope_ = std::move(scope); }
    std::string scope() const { return scope_; }
    auto styles() const { return styles_; }
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
    Element make(std::string type, std::vector<Element> children = {}, std::string classes = {}) {
        if(type=="NativeHost")throw std::invalid_argument("Use nativeHost(name) after registerNative");
        if(type=="Workspace") {
            std::map<std::string,Element> slots;
            for(auto& child:children) {
                if(child.component!="TitleBar" && child.component!="NavigationRail" && child.component!="SessionBar" && child.component!="WorkspaceBody" && child.component!="StatusBar")throw std::invalid_argument("Workspace requires named workspace regions");
                if(!slots.emplace(child.component,child).second)throw std::invalid_argument("Duplicate Workspace region");
            }
            if(!slots.count("WorkspaceBody"))throw std::invalid_argument("Workspace requires WorkspaceBody");
            std::vector<Element> center;
            if(slots.count("SessionBar"))center.push_back(slots.at("SessionBar"));center.push_back(slots.at("WorkspaceBody"));
            auto column=make("Column",center,"workspace-core");column.basis(0).grow();column.as<Stack>()->setGap(0);
            std::vector<Element> middle;if(slots.count("NavigationRail"))middle.push_back(slots.at("NavigationRail"));middle.push_back(column);
            auto row=make("Row",middle,"workspace-core");set(row,"align",L"stretch");set(row,"wrap",false);row.basis(0).grow();row.as<Stack>()->setGap(0);
            std::vector<Element> shell;if(slots.count("TitleBar"))shell.push_back(slots.at("TitleBar"));shell.push_back(row);if(slots.count("StatusBar"))shell.push_back(slots.at("StatusBar"));
            auto built=make("Column",shell,classes);Element result(built,type);result.classes=built.classes;result.grow();styles_->add(result);return result;
        }
        if(type=="DockPanel") {
            int headers=0,bodies=0,footers=0;
            for(auto& child:children){headers+=child.component=="PanelHeader";bodies+=child.component=="PanelBody";footers+=child.component=="PanelFooter";}
            if(headers!=1 || bodies!=1 || footers>1 || headers+bodies+footers!=int(children.size()))throw std::invalid_argument("DockPanel requires one PanelHeader, one PanelBody and optional PanelFooter");
            auto rank=[](const Element& e){return e.component=="PanelHeader"?0:e.component=="PanelBody"?1:2;};
            std::stable_sort(children.begin(),children.end(),[&](const Element& a,const Element& b){return rank(a)<rank(b);});
        }
        if(syntax::pagePattern(type)) {
            // Optional heading stays outside scrolling; a single ActionBar is
            // pinned below the form/detail body. Tables keep native scrolling.
            std::vector<Element> body,footer;
            for(auto& child:children) {
                if(child.component=="ActionBar") footer.push_back(child); else body.push_back(child);
            }
            if(footer.size()>1) throw std::invalid_argument("Page patterns accept at most one ActionBar");
            auto title=make("Text",{},"page-title"), subtitle=make("Text",{},"muted");
            title.widget->setVisible(false);subtitle.widget->setVisible(false);
            std::vector<Element> layout{title,subtitle};
            if(type=="ListPage") layout.insert(layout.end(),body.begin(),body.end());
            else {
                auto content=make("Content",body);
                if(type=="DetailPage") set(content,"max-width",1040);
                layout.push_back(make("Scroll",{content}));
            }
            for(auto& bar:footer) {bar.shrink(0);layout.push_back(bar);}
            auto node=make("Column",layout,classes); Element page(node,type);page.classes=node.classes;
            page.title=title.as<Label>();page.hint=subtitle.as<Label>();page.grow();
            styles_->add(page);return page;
        }
        std::vector<Node> nodes; for (auto& child : children) nodes.push_back(child);
        Builder b(StackEngine::Yoga, "");
        Node node = b.column({});
        std::shared_ptr<Stack> content;
        if (type == "Text" || type == "Status" || type == "ValidationMessage") node = b.paragraph(L"");
        else if (type == "Input" || type == "SearchInput") {
            auto field=std::make_shared<TextField>();field->setClipboard(std::make_shared<SystemClipboard>());
            node=b.native(std::move(field));
        }
        else if (type == "Switch") node = b.native(std::make_shared<Switch>());
        else if (type == "Select") node = b.native(std::make_shared<Select>());
        else if (type == "Button" || type=="ToolButton") node = b.button(L"",{});
        else if(type=="Progress"){auto progress=std::make_shared<ProgressBar>();progress->setPreferredSize({0,6});node=b.native(progress).shrink(0);}
        else if(type=="Tabs") {
            auto tabs=std::make_shared<Tabs>();tabs->setDocumentMode(true);tabs->setSizingMode(TabsSizingMode::Compact);tabs->setItemWidthRange(140,240);tabs->setPreferredSize({0,42});node=b.native(tabs).shrink(0);
        }
        else if(type=="Icon") {auto icon=std::make_shared<IconView>();icon->setPreferredSize({20,20});node=b.native(icon).shrink(0);}
        else if(type=="TimeSeriesChart") {auto chart=std::make_shared<TimeSeriesChart>();chart->setPreferredSize({0,90});chart->setRange(0,100);chart->setGridLines(3);chart->setAxesVisible(false);chart->setLatestPointVisible(true);node=b.native(chart);}
        else if(type=="SplitView") {
            if(children.size()!=2)throw std::invalid_argument("SplitView requires exactly two children");
            auto split=std::make_shared<SplitView>();split->setFirst(children[0].widget);split->setSecond(children[1].widget);split->setGap(5);split->setResizable(true);split->setMinimumPaneExtent(80,80);node=b.native(split).basis(0).grow();
        }
        else if (type == "DataTable") node = b.native(std::make_shared<Table>()).grow();
        else if (type == "Scroll") { if (nodes.size()!=1) throw std::invalid_argument("Scroll needs one child"); node=b.scroll(nodes.front()).grow(); }
        else if(type=="SidebarLayout" || type=="MasterDetail") {
            if(children.size()!=2)throw std::invalid_argument(type+" requires exactly two children");
            if(type=="SidebarLayout" && children.front().component!="Sidebar")throw std::invalid_argument("SidebarLayout requires Sidebar first");
            node=b.native(std::make_shared<AdaptivePanes>(type=="SidebarLayout"?PanePattern::Sidebar:PanePattern::MasterDetail,children[0].widget,children[1].widget)).basis(0).grow();
        }
        else if(type=="Reveal") {
            if(children.size()!=1)throw std::invalid_argument("Reveal needs one child");
            node=b.native(std::make_shared<Reveal>(children.front().widget)).shrink(0);
        }
        else if (type == "Content") {
            auto body = b.column(nodes); body.basis(880).max(880).grow();
            content = body.as<Stack>(); node = b.row({body}); node.as<Stack>()->setAlign(StackAlign::Start);
        }
        else if (type == "Toolbar" || type == "ActionBar" || type == "Row" || type == "FormRow" || type == "FormGrid" || type=="TitleBar" || type=="SessionBar" || type=="PanelHeader" || type=="PanelFooter" || type=="StatusBar" || type=="WorkspaceBody") node = b.flow(nodes);
        else if (syntax::components().count(type)) node = b.column(nodes);
        else throw std::invalid_argument("Unknown component: " + type);
        Element e(node,type); e.classes = classes + " " + scope_; e.content = std::move(content);
        if (type=="Page") e.grow();
        if(type=="DockPanel")for(auto& child:children)if(child.component!="PanelHeader")e.collapsible.push_back(child.widget);
        if(type=="DockPanel" || type=="PanelBody" || type=="WorkspaceBody")e.basis(0).grow();
        if(type=="PanelBody" || type=="DockPanel")e.as<Stack>()->setGap(0);
        if(type=="WorkspaceBody"){e.as<Stack>()->setWrap(false);e.as<Stack>()->setAlign(StackAlign::Stretch);}
        if(type=="TitleBar" || type=="SessionBar" || type=="PanelHeader" || type=="PanelFooter" || type=="StatusBar") {
            e.as<Stack>()->setWrap(false);e.as<Stack>()->setAlign(StackAlign::Center);e.basis(type=="TitleBar"?38:type=="SessionBar"?42:type=="StatusBar"?38:40).shrink(0);
        }
        if(type=="NavigationRail"){e.basis(56).shrink(0);e.as<Stack>()->setAlign(StackAlign::Center);}
        if(type=="Spacer")e.basis(0).grow();
        if(type=="ToolButton")e.widget->setPreferredSize({32,32});

        if(type=="Tabs"){e.tabs=std::make_shared<KeyedTabs>(e.as<Tabs>());own(e.tabs);}
        if(type=="SplitView")e.splitLimits=std::make_shared<Element::SplitLimits>();
        if (type=="SearchInput") e.basis(280).grow();
        if (type=="Select") e.basis(140);
        if (type=="Toolbar" || type=="ActionBar" || type=="Header") e.shrink(0);
        if (type=="Status") e.as<Label>()->setStatusIndicator(6,8);
        if (type=="DataTable") { e.table=std::make_shared<KeyedTable>(e.as<Table>()); own(e.table); e.as<Table>()->setRowHeight(44); e.as<Table>()->setColumnDividersVisible(false); }
        if(type=="FormGrid") {
            for(auto& child:children) {
                if(child.component!="FormRow")throw std::invalid_argument("FormGrid accepts FormRow children only");
                // Grid cells put labels above controls; Yoga owns wrapping and geometry.
                auto row=child.as<Stack>();row->setDirection(StackDirection::Column);row->setWrap(false);row->setAlign(StackAlign::Stretch);
                child.classes+=" grid-field";styles_->add(child);
                StackFlex intrinsic;intrinsic.contentBasis=true;intrinsic.shrink=0;
                for(auto& part:row->children())row->setFlex(part,intrinsic);
                e.as<Stack>()->setFlex(child.widget,{1,1,320,0});
            }
        }
        if (type=="FormRow") {
            if (children.size()!=1) throw std::invalid_argument("FormRow needs one control");
            const bool readOnly=children.front().component=="Text" || children.front().component=="Status";
            auto label=make("Text",{},readOnly?"field-label read-only-label":"field-label"), hint=make("Text",{},"muted");
            hint.widget->setVisible(false);
            auto labels=make("Column",{label,hint},"field-copy"); labels.basis(260).grow();
            auto control=children.front();e.fieldControl=control.widget;
            if(control.component=="Text") {control.classes+=" field-value";styles_->add(control);}
            auto error=make("ValidationMessage");error.widget->setVisible(false);e.error=error.as<Label>();
            auto controlColumn=make("Column",{control,error},"field-control");controlColumn.basis(300).grow();
            e.title=label.as<Label>(); e.hint=hint.as<Label>();
            // Control width is supplied by the column's cross-axis stretch.
            auto controlFlex=control.flex;controlFlex.basis.reset();controlFlex.grow=0;
            controlColumn.as<Stack>()->setFlex(control.widget,controlFlex);
            e.as<Stack>()->reconcileChildren({labels.widget,controlColumn.widget});
            e.as<Stack>()->setFlex(labels.widget,labels.flex); e.as<Stack>()->setFlex(controlColumn.widget,controlColumn.flex);
        } else if (type=="Page" || type=="Header" || type=="Section" || type=="EmptyState" || type=="LoadingState") {
            auto title=make("Text",{},type=="Page" || type=="Header" ? "heading" : "section-title");
            e.title=title.as<Label>(); title.widget->setVisible(false);
            auto stack=e.as<Stack>(); std::vector<std::shared_ptr<Widget>> widgets{title.widget};
            for(auto& child:children) widgets.push_back(child.widget); stack->reconcileChildren(std::move(widgets));
            stack->setFlex(title.widget,title.flex);
            if(type=="EmptyState" || type=="LoadingState") {
                auto hint=make("Text",{},"muted");hint.widget->setVisible(false);e.hint=hint.as<Label>();
                std::vector<std::shared_ptr<Widget>> content{title.widget,hint.widget};for(auto& child:children)content.push_back(child.widget);
                stack->reconcileChildren(std::move(content));stack->setFlex(hint.widget,hint.flex);
                if(type=="LoadingState") {e.title->setText(L"正在加载…");e.title->setVisible(true);}
            }
        }
        styles_->add(e); return e;
    }
    void set(Element& e, const std::string& key, const std::wstring& value) {
        if(key=="icon" || key=="symbol") {
            const auto symbol=namedIcon(std::string(value.begin(),value.end()));
            if(!symbol)throw std::invalid_argument("Unknown authoring icon");
            if(key=="icon")e.as<Button>()->setIcon(*symbol);else e.as<IconView>()->setSymbol(*symbol);
        } else if(key=="presentation") {
            if(value!=L"document" && value!=L"segmented")throw std::invalid_argument("Tabs presentation must be document or segmented");
            auto tabs=e.as<Tabs>();tabs->setDocumentMode(value==L"document");tabs->setSizingMode(value==L"document"?TabsSizingMode::Compact:TabsSizingMode::Equal);
        } else if(key=="orientation") {
            if(value!=L"horizontal" && value!=L"vertical")throw std::invalid_argument("orientation must be horizontal or vertical");
            e.as<SplitView>()->setOrientation(value==L"horizontal"?SplitOrientation::Horizontal:SplitOrientation::Vertical);
        } else if(key=="align") {
            if(e.content) {
                if(value!=L"start" && value!=L"center" && value!=L"end")throw std::invalid_argument("Content align must be start, center or end");
                e.as<Stack>()->setJustify(value==L"center" ? StackJustify::Center : value==L"end" ? StackJustify::End : StackJustify::Start);
            } else if(e.component=="Row" || e.component=="Column") {
                if(value!=L"start" && value!=L"center" && value!=L"end" && value!=L"stretch")throw std::invalid_argument("align must be start, center, end or stretch");
                e.as<Stack>()->setAlign(value==L"start"?StackAlign::Start:value==L"center"?StackAlign::Center:value==L"end"?StackAlign::End:StackAlign::Stretch);
            } else throw std::invalid_argument("align requires Row, Column or Content");
        } else if(key=="justify") {
            if(e.component!="Row" && e.component!="Column")throw std::invalid_argument("justify requires Row or Column");
            if(value!=L"start" && value!=L"center" && value!=L"end" && value!=L"space-between")throw std::invalid_argument("justify must be start, center, end or space-between");
            e.as<Stack>()->setJustify(value==L"center"?StackJustify::Center:value==L"end"?StackJustify::End:value==L"space-between"?StackJustify::SpaceBetween:StackJustify::Start);
        }
        else if(key=="variant") styles_->variant(e,value);
        else if(key=="appearance") styles_->appearance(e,value);
        else if(key=="tone") {if(e.component!="Status")throw std::invalid_argument("tone requires Status");styles_->tone(e,value);}
        else if(key=="subtitle") {if(!e.hint)throw std::invalid_argument("Component has no subtitle");e.hint->setText(value);e.hint->setVisible(!value.empty());}
        else if(key=="error") {
            if(!e.error || !e.fieldControl)throw std::invalid_argument("error requires FormRow");
            e.error->setText(value);e.error->setVisible(!value.empty());
            styles_->invalid(e.fieldControl,!value.empty());
            e.fieldControl->setAccessibleDescription(e.hint->text()+(value.empty()?L"":L" "+value));
        }
        else if(key=="name") {e.widget->setAccessibleName(value);if(e.component=="ToolButton")e.widget->setTooltip(value);}
        else if(key=="description") e.widget->setAccessibleDescription(value);
        else if(key=="title" || key=="label") {
            if(!e.title) throw std::invalid_argument("Component has no title"); e.title->setText(value); e.title->setVisible(!value.empty());
            if(key=="label" && e.fieldControl) e.fieldControl->setAccessibleName(value);
        } else if(key=="hint") {
            e.hint->setText(value);e.hint->setVisible(!value.empty());
            if(e.fieldControl)e.fieldControl->setAccessibleDescription(value+(e.error && !e.error->text().empty()?L" "+e.error->text():L""));
        } else if(key=="preset") {
            if(value!=L"fade" && value!=L"expand")throw std::invalid_argument("Reveal preset must be fade or expand");
            e.as<Reveal>()->setPreset(value==L"fade"?RevealPreset::Fade:RevealPreset::Expand);
        } else if(key=="placeholder") e.as<TextField>()->setPlaceholder(value);
        else if(key=="selectedKey") {if(e.tabs)e.tabs->select(value);else e.table->select(value);}
        else if(key=="text") {
            if(auto w=e.as<Label>()) w->setText(value);
            else if(auto w=e.as<Button>()) {w->setText(value);if(e.component=="ToolButton")w->setPreferredSize({value.empty()?32.0f:0.0f,32});}
            else if(auto w=e.as<Switch>()) w->setText(value);
            else if(auto w=e.as<TextField>()) { if(w->text()!=value) w->setText(value); }
            if(e.component=="ValidationMessage") e.widget->setVisible(!value.empty());
        } else throw std::invalid_argument("Invalid text property: " + key);
    }
    void set(Element& e,const std::string& key,const wchar_t* value) { set(e,key,std::wstring(value)); }
    void set(Element& e,const std::string& key,const std::string& value) {
        if(key=="class") { e.classes += " " + value; styles_->add(e); }
        else set(e,key,wide(value));
    }
    void set(Element& e,const std::string& key,bool value) {
        if(key=="visible") e.widget->setVisible(value); else if(key=="disabled") e.widget->setDisabled(value);
        else if(key=="collapsed"){for(auto& part:e.collapsible)part->setVisible(!value);}
        else if(key=="second-collapsed")e.as<SplitView>()->setSecondCollapsed(value);
        else if(key=="closable")e.as<Tabs>()->setClosable(value);
        else if(key=="themed"){e.as<NativeHostView>()->setThemed(value);styles_->add(e);}
        else if(key=="wrap") {
            if(e.component!="Row" && e.component!="Column")throw std::invalid_argument("wrap requires Row or Column");
            e.as<Stack>()->setWrap(value);
        }
        else if(key=="detail-open") e.as<AdaptivePanes>()->setDetailOpen(value);
        else if(key=="open") e.as<Reveal>()->setOpen(value);
        else if(key=="reduced-motion") e.as<Reveal>()->setReducedMotion(value);
        else if(key=="checked") e.as<Switch>()->setChecked(value); else throw std::invalid_argument("Invalid bool property");
    }
    void set(Element& e,const std::string& key,float value) {
        syntax::validateLayoutNumber(key,value);
        if(key=="value"){e.as<ProgressBar>()->setValue(std::clamp(value,0.0f,1.0f));return;}
        if(key=="collapsed-extent"){e.as<SplitView>()->setCollapsedExtent(value);return;}
        if(key=="size"){e.widget->setPreferredSize({value,value});return;}
        if(key=="ratio"){e.as<SplitView>()->setSplitRatio(value);return;}
        if(key=="first-min" || key=="second-min") {
            if(key=="first-min")e.splitLimits->first=value;else e.splitLimits->second=value;
            e.as<SplitView>()->setMinimumPaneExtent(e.splitLimits->first,e.splitLimits->second);return;
        }
        if(key=="breakpoint") {e.as<AdaptivePanes>()->setBreakpoint(value);return;}
        if(key=="pane-width") {e.as<AdaptivePanes>()->setPaneWidth(value);return;}
        if(key=="min-column-width") {
            if(e.component!="FormGrid")throw std::invalid_argument("min-column-width requires FormGrid");
            for(auto& child:e.as<Stack>()->children())e.as<Stack>()->setFlex(child,{1,1,value,0});
            return;
        }
        if(key=="max-width") {
            if(!e.content) throw std::invalid_argument("max-width is only supported on Content");
            e.as<Stack>()->setFlex(e.content,{1,1,value,0,value,true}); return;
        }
        if(key=="grow") e.grow(value); else if(key=="basis") e.basis(value); else if(key=="min") e.min(value); else if(key=="max") e.max(value); else if(key=="shrink") e.shrink(value); else throw std::invalid_argument("Invalid layout property");
    }
    void set(Element& e,const std::string& key,int value) { if(key=="selectedIndex") e.as<Select>()->setSelectedIndex(value); else set(e,key,float(value)); }
    void set(Element& e,const std::string&,const std::vector<std::wstring>& items) { e.as<Select>()->setItems(items); }
    void set(Element& e,const std::string&,const std::vector<TableColumn>& columns) { e.as<Table>()->setColumns(columns); }
    void set(Element& e,const std::string&,const std::vector<TableRow>& rows) { e.table->update(rows); }
    void set(Element& e,const std::string&,const std::vector<TabItem>& items){e.tabs->update(items);}
    void set(Element& e,const std::string&,const std::vector<TimeSeriesChartSeries>& series){e.as<TimeSeriesChart>()->setSeries(series);}
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
        auto weak=std::weak_ptr<Life>(life_); e.as<Button>()->setOnClick([weak,&command] { if(!weak.expired()) command.execute(); });
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
        auto result=make("Column"); auto styles=styles_; auto schedule=schedule_; auto scope=scope_; auto natives=natives_;
        watch(source,[rows,result,styles,schedule,scope,natives,key,factory](const Items& items) mutable {
            std::set<std::wstring> keys; for(auto& item:items) if(!item || key(item).empty() || !keys.insert(key(item)).second) throw std::invalid_argument("v-for requires nonnull shared items and unique nonempty keys");
            std::map<std::wstring,Row> next; std::vector<std::shared_ptr<Widget>> widgets;
            for(auto& item:items) {
                auto id=key(item); auto found=rows->find(id);
                if(found!=rows->end()) {
                    if(found->second.item!=item) throw std::invalid_argument("A retained v-for key must retain its shared item identity; mutate the item's State fields");
                    next.emplace(id,std::move(found->second));
                } else { auto mount=std::make_shared<Mount>(schedule,styles); mount->setScope(scope); mount->natives_=natives; auto element=factory(*mount,item); next.emplace(id,Row{item,mount,element}); }
                auto& row=next.at(id); widgets.push_back(row.element.widget); result.template as<Stack>()->setFlex(row.element.widget,row.element.flex);
            }
            result.template as<Stack>()->reconcileChildren(std::move(widgets)); *rows=std::move(next);
        });
        childFlush_.push_back([rows] { for(auto& row:*rows) row.second.mount->flush(); }); own(rows); return result;
    }
};
using Slots = std::map<std::string,std::function<std::vector<Element>(Mount&)>>;
} // namespace oneui::ui
