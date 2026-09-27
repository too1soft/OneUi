#pragma once
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
    std::string component, classes;
    std::shared_ptr<Label> title, hint, error;
    std::shared_ptr<Widget> fieldControl;
    std::shared_ptr<KeyedTable> table;
    std::shared_ptr<Stack> content;
    Element(Node node, std::string type) : Node(std::move(node)), component(std::move(type)) {}
};

class StyleRegistry {
public:
    struct Source { std::string component, file; int line=0; };
private:
    struct Entry { std::weak_ptr<Widget> widget; std::string component, classes; std::weak_ptr<Stack> content; std::string tone, file; int line=0; bool invalid=false; };
    std::vector<Entry> entries_;
    StyleSheet sheet_;
    Density density_=Density::Comfortable;
    void apply(const Entry& entry, const StyleSheet& sheet) {
        auto w = entry.widget.lock(); if (!w) return;
        StyleNode n; n.tag = syntax::components().at(entry.component); n.classes.push_back("one-" + entry.component);
        std::istringstream tokens(entry.classes); for (std::string c; tokens >> c;) n.classes.push_back(c);
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
    }
public:
    void invalid(const std::shared_ptr<Widget>& widget,bool value) {
        for(auto& entry:entries_)if(entry.widget.lock()==widget) {
            if(entry.invalid!=value){entry.invalid=value;apply(entry,sheet_);}return;
        }
    }
    void locate(const Element& e,std::string file,int line) {
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
        entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[](auto& entry){return entry.widget.expired();}),entries_.end());
        for (auto& entry : entries_) if (entry.widget.lock() == e.widget) { entry.component=e.component;entry.classes = e.classes; apply(entry,sheet_); return; }
        entries_.push_back({e.widget,e.component,e.classes,e.content}); apply(entries_.back(),sheet_);
    }
    void replace(const std::string& validatedCss,Density density=Density::Comfortable) {
        StyleSheet next; std::string error;
        if (!next.addRulesFromCss(validatedCss,&error)) throw std::invalid_argument(error);
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
        else if (type == "Button") node = b.button(L"",{});
        else if (type == "DataTable") node = b.native(std::make_shared<Table>()).grow();
        else if (type == "Scroll") { if (nodes.size()!=1) throw std::invalid_argument("Scroll needs one child"); node=b.scroll(nodes.front()).grow(); }
        else if (type == "Content") {
            auto body = b.column(nodes); body.basis(880).max(880).grow();
            content = body.as<Stack>(); node = b.row({body}); node.as<Stack>()->setAlign(StackAlign::Start);
        }
        else if (type == "Toolbar" || type == "ActionBar" || type == "Row" || type == "FormRow" || type == "FormGrid") node = b.flow(nodes);
        else if (syntax::components().count(type)) node = b.column(nodes);
        else throw std::invalid_argument("Unknown component: " + type);
        Element e(node,type); e.classes = classes + " " + scope_; e.content = std::move(content);
        if (type=="Page") e.grow();
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
            auto label=make("Text",{},"field-label"), hint=make("Text",{},"muted");
            hint.widget->setVisible(false);
            auto labels=make("Column",{label,hint},"field-copy"); labels.basis(260).grow();
            auto control=children.front();e.fieldControl=control.widget;
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
        if(key=="align") {
            if(!e.content || (value!=L"start" && value!=L"center" && value!=L"end")) throw std::invalid_argument("Content align must be start, center or end");
            e.as<Stack>()->setJustify(value==L"center" ? StackJustify::Center : value==L"end" ? StackJustify::End : StackJustify::Start);
        }
        else if(key=="tone") {if(e.component!="Status")throw std::invalid_argument("tone requires Status");styles_->tone(e,value);}
        else if(key=="subtitle") {if(!e.hint)throw std::invalid_argument("Component has no subtitle");e.hint->setText(value);e.hint->setVisible(!value.empty());}
        else if(key=="error") {
            if(!e.error || !e.fieldControl)throw std::invalid_argument("error requires FormRow");
            e.error->setText(value);e.error->setVisible(!value.empty());
            styles_->invalid(e.fieldControl,!value.empty());
            e.fieldControl->setAccessibleDescription(e.hint->text()+(value.empty()?L"":L" "+value));
        }
        else if(key=="name") e.widget->setAccessibleName(value);
        else if(key=="description") e.widget->setAccessibleDescription(value);
        else if(key=="title" || key=="label") {
            if(!e.title) throw std::invalid_argument("Component has no title"); e.title->setText(value); e.title->setVisible(!value.empty());
            if(key=="label" && e.fieldControl) e.fieldControl->setAccessibleName(value);
        } else if(key=="hint") {
            e.hint->setText(value);e.hint->setVisible(!value.empty());
            if(e.fieldControl)e.fieldControl->setAccessibleDescription(value+(e.error && !e.error->text().empty()?L" "+e.error->text():L""));
        } else if(key=="placeholder") e.as<TextField>()->setPlaceholder(value);
        else if(key=="selectedKey") e.table->select(value);
        else if(key=="text") {
            if(auto w=e.as<Label>()) w->setText(value);
            else if(auto w=e.as<Button>()) w->setText(value);
            else if(auto w=e.as<Switch>()) w->setText(value);
            else if(auto w=e.as<TextField>()) { if(w->text()!=value) w->setText(value); }
            if(e.component=="ValidationMessage") e.widget->setVisible(!value.empty());
        } else throw std::invalid_argument("Invalid text property: " + key);
    }
    void set(Element& e,const std::string& key,const wchar_t* value) { set(e,key,std::wstring(value)); }
    void set(Element& e,const std::string& key,const std::string& value) {
        if(key=="class" || key=="variant") { e.classes += " " + value; styles_->add(e); }
        else set(e,key,wide(value));
    }
    void set(Element& e,const std::string& key,bool value) {
        if(key=="visible") e.widget->setVisible(value); else if(key=="disabled") e.widget->setDisabled(value);
        else if(key=="checked") e.as<Switch>()->setChecked(value); else throw std::invalid_argument("Invalid bool property");
    }
    void set(Element& e,const std::string& key,float value) {
        syntax::validateLayoutNumber(key,value);
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
    template<class Source> void bind(Element e,const std::string& key,Source& source) {
        watch(source,[this,e,key](const auto& v) mutable { set(e,key,v); });
    }
    template<class T,class Source> void bindTyped(Element e,const std::string& key,Source& source) {
        static_assert(std::is_same_v<T,std::decay_t<decltype(source.get())>>, "Template property and ViewModel type do not match");
        bind(e,key,source);
    }
    void model(Element e,State<std::wstring>& source) {
        if(e.table) {
            auto binding=std::make_shared<Binding<std::wstring>>(source); own(binding);
            e.table->selectionValue=[binding] {const std::wstring fallback; return std::wstring(binding->get(fallback));};
            bind(e,"selectedKey",source); e.table->selected=[binding](std::wstring key) { std::wstring fallback; binding->set(std::move(key),fallback); };
        } else { e.as<TextField>()->bindText(source); }
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
        auto result=make("Column"); auto styles=styles_; auto schedule=schedule_; auto scope=scope_;
        watch(source,[rows,result,styles,schedule,scope,key,factory](const Items& items) mutable {
            std::set<std::wstring> keys; for(auto& item:items) if(!item || key(item).empty() || !keys.insert(key(item)).second) throw std::invalid_argument("v-for requires nonnull shared items and unique nonempty keys");
            std::map<std::wstring,Row> next; std::vector<std::shared_ptr<Widget>> widgets;
            for(auto& item:items) {
                auto id=key(item); auto found=rows->find(id);
                if(found!=rows->end()) {
                    if(found->second.item!=item) throw std::invalid_argument("A retained v-for key must retain its shared item identity; mutate the item's State fields");
                    next.emplace(id,std::move(found->second));
                } else { auto mount=std::make_shared<Mount>(schedule,styles); mount->setScope(scope); auto element=factory(*mount,item); next.emplace(id,Row{item,mount,element}); }
                auto& row=next.at(id); widgets.push_back(row.element.widget); result.template as<Stack>()->setFlex(row.element.widget,row.element.flex);
            }
            result.template as<Stack>()->reconcileChildren(std::move(widgets)); *rows=std::move(next);
        });
        childFlush_.push_back([rows] { for(auto& row:*rows) row.second.mount->flush(); }); own(rows); return result;
    }
};
using Slots = std::map<std::string,std::function<std::vector<Element>(Mount&)>>;
} // namespace oneui::ui
