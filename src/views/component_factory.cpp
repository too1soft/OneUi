#include "oneui/ui_declarative.h"
#include "data_menu.h"
#include "oneui/controls/popup.h"
#include "oneui/layout/panel.h"
namespace oneui::ui {
Element Mount::make(std::string type, std::vector<Element> children, std::string classes) {
    if (type == "NativeHost")
        throw std::invalid_argument("Use nativeHost(name) after registerNative");
    if (type == "Workspace") {
        std::map<std::string, Element> slots;
        for (auto &child : children) {
            if (child.component != "TitleBar" && child.component != "NavigationRail" &&
                child.component != "SessionBar" && child.component != "WorkspaceBody" &&
                child.component != "StatusBar")
                throw std::invalid_argument("Workspace requires named workspace regions");
            if (!slots.emplace(child.component, child).second)
                throw std::invalid_argument("Duplicate Workspace region");
        }
        if (!slots.count("WorkspaceBody"))
            throw std::invalid_argument("Workspace requires WorkspaceBody");
        std::vector<Element> center;
        if (slots.count("SessionBar"))
            center.push_back(slots.at("SessionBar"));
        center.push_back(slots.at("WorkspaceBody"));
        auto column = make("Column", center, "workspace-core");
        column.basis(0).grow();
        column.as<Stack>()->setGap(0);
        std::vector<Element> middle;
        if (slots.count("NavigationRail"))
            middle.push_back(slots.at("NavigationRail"));
        middle.push_back(column);
        auto row = make("Row", middle, "workspace-core");
        set(row, "align", L"stretch");
        set(row, "wrap", false);
        row.basis(0).grow();
        row.as<Stack>()->setGap(0);
        std::vector<Element> shell;
        if (slots.count("TitleBar"))
            shell.push_back(slots.at("TitleBar"));
        shell.push_back(row);
        if (slots.count("StatusBar"))
            shell.push_back(slots.at("StatusBar"));
        auto built = make("Column", shell, classes);
        Element result(built, type);
        result.classes = built.classes;
        result.grow();
        styles_->add(result);
        return result;
    }
    if (type == "DockPanel") {
        int headers = 0, bodies = 0, footers = 0;
        for (auto &child : children) {
            headers += child.component == "PanelHeader";
            bodies += child.component == "PanelBody";
            footers += child.component == "PanelFooter";
        }
        if (headers != 1 || bodies != 1 || footers > 1 ||
            headers + bodies + footers != int(children.size()))
            throw std::invalid_argument(
                "DockPanel requires one PanelHeader, one PanelBody and optional PanelFooter");
        auto rank = [](const Element &e) {
            return e.component == "PanelHeader" ? 0 : e.component == "PanelBody" ? 1 : 2;
        };
        std::stable_sort(children.begin(), children.end(),
                         [&](const Element &a, const Element &b) { return rank(a) < rank(b); });
    }
    if (syntax::pagePattern(type)) {
        // Optional heading stays outside scrolling; a single ActionBar is
        // pinned below the form/detail body. Tables keep native scrolling.
        std::vector<Element> body, footer;
        for (auto &child : children) {
            if (child.component == "ActionBar")
                footer.push_back(child);
            else
                body.push_back(child);
        }
        if (footer.size() > 1)
            throw std::invalid_argument("Page patterns accept at most one ActionBar");
        auto title = make("Text", {}, "page-title"), subtitle = make("Text", {}, "muted");
        title.widget->setVisible(false);
        subtitle.widget->setVisible(false);
        std::vector<Element> layout{title, subtitle};
        if (type == "ListPage")
            layout.insert(layout.end(), body.begin(), body.end());
        else {
            auto content = make("Content", body);
            if (type == "DetailPage")
                set(content, "max-width", 1040);
            layout.push_back(make("Scroll", {content}));
        }
        for (auto &bar : footer) {
            bar.shrink(0);
            layout.push_back(bar);
        }
        auto node = make("Column", layout, classes);
        Element page(node, type);
        page.classes = node.classes;
        page.title = title.as<Label>();
        page.hint = subtitle.as<Label>();
        page.grow();
        styles_->add(page);
        return page;
    }
    std::vector<Node> nodes;
    for (auto &child : children)
        nodes.push_back(child);
    Builder b(StackEngine::Yoga, "");
    Node node = b.column({});
    std::shared_ptr<Stack> content;
    if(type=="GridItem") {
        if(children.size()!=1)throw std::invalid_argument("GridItem requires one child");
        node=b.native(std::make_shared<TemplateGridItem>(children[0].widget));
    }
    else if(type=="ReorderableGrid") {
        auto grid=std::make_shared<ReorderableGrid>();
        std::vector<std::shared_ptr<Widget>> items;
        for(const auto& child:children)items.push_back(child.widget);
        grid->setItems(templateGridItems(items));
        node=b.native(grid);
    }
    else if(type=="Overlay") {
        if(children.size()!=2)throw std::invalid_argument("Overlay requires base and foreground");
        node=b.native(std::make_shared<TemplateOverlay>(children[0].widget,children[1].widget));
    }
    else if(type=="TemplateTable") {
        if(children.size()!=2)throw std::invalid_argument("TemplateTable requires header and body");
        node=b.native(std::make_shared<TemplateTable>(children[0].widget,children[1].widget)).basis(0).grow();
    }
    else if(type=="Dialog") {
        if(children.size()!=2)throw std::invalid_argument("Dialog requires content and actions");
        auto dialog=std::make_shared<Dialog>();
        dialog->setContent(children[0].widget);
        dialog->setActions(children[1].widget);
        node=b.native(dialog);
    }
    else if(type=="Popup") {
        if(children.size()!=1)throw std::invalid_argument("Popup requires one content child");
        auto popup=std::make_shared<Popup>();
        popup->setContent(children[0].widget);
        popup->setInteractionMode(PopupInteractionMode::LightDismiss);
        node=b.native(popup);
    }
    else if(type=="DataMenu")node=b.native(std::make_shared<views::DataMenu>());
    else if(type=="List")node=b.native(std::make_shared<List>());
    else if(type=="VirtualList")node=b.native(std::make_shared<VirtualList>());
    else if(type=="FontPicker")node=b.native(std::make_shared<FontPicker>());
    else if(type=="Slider")node=b.native(std::make_shared<Slider>());
    else if(type=="InteractiveSurface") {
        auto surface=std::make_shared<InteractiveSurface>();
        if(children.size()!=1)throw std::invalid_argument("InteractiveSurface requires one child");
        surface->setContent(children[0].widget);node=b.native(surface);
    } else if(type=="Card") {
        auto card=std::make_shared<Card>();if(children.size()!=1)throw std::invalid_argument("Card requires one child");card->setContent(children[0].widget);node=b.native(card);
    } else if(type=="ThemeScope") {
        auto scope=std::make_shared<ThemeScope>();if(children.size()!=1)throw std::invalid_argument("ThemeScope requires one child");scope->add(children[0].widget);node=b.native(scope);
    } else if(type=="MediaSlot") {
        auto slot=std::make_shared<MediaSlotView>();for(auto& child:children)slot->add(child.widget);node=b.native(slot);
    } else if(type=="MediaPicker"||type=="MediaPreview") {
        std::set<std::string> slots;
        auto picker=type=="MediaPicker"?std::make_shared<MediaPicker>():nullptr;
        auto preview=type=="MediaPreview"?std::make_shared<MediaPreview>():nullptr;
        for(auto& child:children) {
            auto slot=child.as<MediaSlotView>();if(!slot||!slots.insert(slot->name).second)throw std::invalid_argument("Unique named media slots required");
            if(picker)picker->setSlot(slot->name,slot);else preview->setSlot(slot->name,slot);
        }
        node=picker?b.native(picker):b.native(preview);
    } else if(type=="ContainerPage") {
        auto page=std::make_shared<ContainerPage>();for(auto& child:children)page->add(child.widget);node=b.native(page);
    } else if(type=="DragHandle" || type=="DropSurface") {
        std::shared_ptr<View> surface=type=="DragHandle"?std::static_pointer_cast<View>(std::make_shared<DragHandle>()):std::static_pointer_cast<View>(std::make_shared<DropSurface>());
        for(auto& child:children)surface->add(child.widget);
        node=b.native(surface);
    } else if(type=="HeaderSlot") {
        auto slot=std::make_shared<PaneHeaderSlot>();
        for(auto& child:children)slot->add(child.widget);
        node=b.native(slot);
    } else if(type=="PaneHeader") {
        auto header=std::make_shared<PaneHeader>();std::set<std::string> names;
        for(auto& child:children) {
            auto slot=child.as<PaneHeaderSlot>();
            if(!names.insert(slot->name).second)throw std::invalid_argument("Duplicate PaneHeader slot");
            header->setSlot(slot->name,slot);
        }
        node=b.native(header);
    } else if (type == "Text" || type == "Status" || type == "ValidationMessage")
        node = b.paragraph(L"");
    else if (type == "TextArea") {
        auto field = std::make_shared<TextArea>();
        field->setClipboard(clipboard_);
        node = b.native(std::move(field));
    } else if (type == "Checkbox")
        node = b.native(std::make_shared<Checkbox>());
    else if (type == "Input" || type == "SearchInput") {
        auto field = std::make_shared<TextField>();
        field->setClipboard(clipboard_);
        node = b.native(std::move(field));
    } else if (type == "Switch")
        node = b.native(std::make_shared<Switch>());
    else if (type == "Select")
        node = b.native(std::make_shared<Select>());
    else if (type == "Button" || type == "ToolButton")
        node = b.button(L"", {});
    else if (type == "Progress") {
        auto progress = std::make_shared<ProgressBar>();
        progress->setPreferredSize({0, 6});
        node = b.native(progress).shrink(0);
    } else if (type == "Tabs" || type == "SegmentedControl") {
        auto tabs = std::make_shared<Tabs>();
        tabs->setDocumentMode(true);
        tabs->setSizingMode(TabsSizingMode::Compact);
        tabs->setItemWidthRange(140, 240);
        tabs->setPreferredSize({0, 42});
        node = b.native(tabs).shrink(0);
    } else if (type == "Icon") {
        auto icon = std::make_shared<IconView>();
        icon->setPreferredSize({20, 20});
        node = b.native(icon).shrink(0);
    } else if (type == "TimeSeriesChart") {
        auto chart = std::make_shared<TimeSeriesChart>();
        chart->setPreferredSize({0, 90});
        chart->setRange(0, 100);
        chart->setGridLines(3);
        chart->setAxesVisible(false);
        chart->setLatestPointVisible(true);
        node = b.native(chart);
    } else if (type == "SplitView") {
        if (children.size() != 2)
            throw std::invalid_argument("SplitView requires exactly two children");
        auto split = std::make_shared<SplitView>();
        split->setFirst(children[0].widget);
        split->setSecond(children[1].widget);
        split->setGap(5);
        split->setResizable(true);
        split->setMinimumPaneExtent(80, 80);
        node = b.native(split).basis(0).grow();
    } else if (type == "DataTable")
        node = b.native(std::make_shared<Table>()).grow();
    else if (type == "Scroll") {
        if (nodes.size() != 1)
            throw std::invalid_argument("Scroll needs one child");
        node = b.scroll(nodes.front()).grow();
    } else if (type == "SidebarLayout" || type == "MasterDetail") {
        if (children.size() != 2)
            throw std::invalid_argument(type + " requires exactly two children");
        if (type == "SidebarLayout" && children.front().component != "Sidebar")
            throw std::invalid_argument("SidebarLayout requires Sidebar first");
        node =
            b.native(std::make_shared<AdaptivePanes>(
                         type == "SidebarLayout" ? PanePattern::Sidebar : PanePattern::MasterDetail,
                         children[0].widget, children[1].widget))
                .basis(0)
                .grow();
    } else if (type == "Reveal") {
        if (children.size() != 1)
            throw std::invalid_argument("Reveal needs one child");
        node = b.native(std::make_shared<Reveal>(children.front().widget)).shrink(0);
    } else if (type == "Content") {
        auto body = b.column(nodes);
        body.basis(880).max(880).grow();
        content = body.as<Stack>();
        node = b.row({body});
        node.as<Stack>()->setAlign(StackAlign::Start);
    } else if (type == "Toolbar" || type == "ActionBar" || type == "Row" || type == "FormRow" ||
               type == "FormGrid" || type == "TitleBar" || type == "SessionBar" ||
               type == "PanelHeader" || type == "PanelFooter" || type == "StatusBar" ||
               type == "WorkspaceBody")
        node = b.flow(nodes);
    else if (type == "MountPoint")
        node = b.native(std::make_shared<Panel>());
    else if (syntax::components().count(type))
        node = b.column(nodes);
    else
        throw std::invalid_argument("Unknown component: " + type);
    Element e(node, type);
    e.classes = classes + " " + scope_;
    e.content = std::move(content);
    if (type == "Page")
        e.grow();
    if (type == "DockPanel")
        for (auto &child : children)
            if (child.component != "PanelHeader")
                e.collapsible.push_back(child.widget);
    if (type == "DockPanel" || type == "PanelBody" || type == "WorkspaceBody")
        e.basis(0).grow();
    if (type == "PanelBody" || type == "DockPanel")
        e.as<Stack>()->setGap(0);
    if (type == "WorkspaceBody") {
        e.as<Stack>()->setWrap(false);
        e.as<Stack>()->setAlign(StackAlign::Stretch);
    }
    if (type == "TitleBar" || type == "SessionBar" || type == "PanelHeader" ||
        type == "PanelFooter" || type == "StatusBar") {
        e.as<Stack>()->setWrap(false);
        e.as<Stack>()->setAlign(StackAlign::Center);
        e.basis(type == "TitleBar"     ? 38
                : type == "SessionBar" ? 42
                : type == "StatusBar"  ? 38
                                       : 40)
            .shrink(0);
    }
    if (type == "NavigationRail") {
        e.basis(56).shrink(0);
        e.as<Stack>()->setAlign(StackAlign::Center);
    }
    if (type == "Spacer")
        e.basis(0).grow();
    if (type == "ToolButton")
        e.widget->setPreferredSize({32, 32});

    if (type == "SegmentedControl") {
        e.as<Tabs>()->setDocumentMode(false);
        e.as<Tabs>()->setSizingMode(TabsSizingMode::Equal);
    }
    if (type == "Tabs") {
        e.tabs = std::make_shared<KeyedTabs>(e.as<Tabs>());
        own(e.tabs);
    }
    if (type == "SplitView")
        e.splitLimits = std::make_shared<Element::SplitLimits>();
    if (type == "SearchInput")
        e.basis(280).grow();
    // Native Select supplies a width preference. A main-axis basis would turn
    // that width into a 140px height inside an imported slot's column wrapper.
    if (type == "Toolbar" || type == "ActionBar" || type == "Header")
        e.shrink(0);
    if (type == "Status")
        e.as<Label>()->setStatusIndicator(6, 8);
    if (type == "DataTable") {
        e.table = std::make_shared<KeyedTable>(e.as<Table>());
        own(e.table);
        e.as<Table>()->setRowHeight(44);
        e.as<Table>()->setColumnDividersVisible(false);
    }
    if (type == "FormGrid") {
        for (auto &child : children) {
            if (child.component != "FormRow")
                throw std::invalid_argument("FormGrid accepts FormRow children only");
            // Grid cells put labels above controls; Yoga owns wrapping and geometry.
            auto row = child.as<Stack>();
            row->setDirection(StackDirection::Column);
            row->setWrap(false);
            row->setAlign(StackAlign::Stretch);
            child.classes += " grid-field";
            styles_->add(child);
            StackFlex intrinsic;
            intrinsic.contentBasis = true;
            intrinsic.shrink = 0;
            for (auto &part : row->children())
                row->setFlex(part, intrinsic);
            e.as<Stack>()->setFlex(child.widget, {1, 1, 320, 0});
        }
    }
    if (type == "FormRow") {
        if (children.size() != 1)
            throw std::invalid_argument("FormRow needs one control");
        const bool readOnly =
            children.front().component == "Text" || children.front().component == "Status";
        auto label = make("Text", {}, readOnly ? "field-label read-only-label" : "field-label"),
             hint = make("Text", {}, "muted");
        hint.widget->setVisible(false);
        auto labels = make("Column", {label, hint}, "field-copy");
        labels.basis(260).grow();
        auto control = children.front();
        e.fieldControl = control.widget;
        if (control.component == "Text") {
            control.classes += " field-value";
            styles_->add(control);
        }
        auto error = make("ValidationMessage");
        error.widget->setVisible(false);
        e.error = error.as<Label>();
        auto controlColumn = make("Column", {control, error}, "field-control");
        controlColumn.basis(300).grow();
        e.title = label.as<Label>();
        e.hint = hint.as<Label>();
        // Control width is supplied by the column's cross-axis stretch.
        auto controlFlex = control.flex;
        controlFlex.basis.reset();
        controlFlex.grow = 0;
        controlColumn.as<Stack>()->setFlex(control.widget, controlFlex);
        e.as<Stack>()->reconcileChildren({labels.widget, controlColumn.widget});
        e.as<Stack>()->setFlex(labels.widget, labels.flex);
        e.as<Stack>()->setFlex(controlColumn.widget, controlColumn.flex);
    } else if (type == "Page" || type == "Header" || type == "Section" || type == "EmptyState" ||
               type == "LoadingState") {
        auto title =
            make("Text", {}, type == "Page" || type == "Header" ? "heading" : "section-title");
        e.title = title.as<Label>();
        title.widget->setVisible(false);
        auto stack = e.as<Stack>();
        std::vector<std::shared_ptr<Widget>> widgets{title.widget};
        for (auto &child : children)
            widgets.push_back(child.widget);
        stack->reconcileChildren(std::move(widgets));
        stack->setFlex(title.widget, title.flex);
        if (type == "EmptyState" || type == "LoadingState") {
            auto hint = make("Text", {}, "muted");
            hint.widget->setVisible(false);
            e.hint = hint.as<Label>();
            std::vector<std::shared_ptr<Widget>> content{title.widget, hint.widget};
            for (auto &child : children)
                content.push_back(child.widget);
            stack->reconcileChildren(std::move(content));
            stack->setFlex(hint.widget, hint.flex);
            if (type == "LoadingState") {
                e.title->setText(builtinText(BuiltinText::Loading));
                e.title->setVisible(true);
            }
        }
    }
    styles_->add(e);
    return e;
}
} // namespace oneui::ui

namespace oneui::ui {
void StyleRegistry::inheritTransient(const std::shared_ptr<StyleSheet>& sheet) {
    sheet_.setTransientStyleSheet(sheet);
    // Only Select consumes the separate popup palette. Leave repeat flow intact.
    for(const auto& entry:entries_)if(entry.component=="Select")apply(entry,sheet_);
}
namespace {
#if ONEUI_DEVELOPER_TOOLS
thread_local std::map<const Widget*,StyleDiagnostic> diagnostics;
#endif
}
void rememberStyleDiagnostic(const std::shared_ptr<Widget>& widget,const StyleNode& node,const StyleBox& box) {
#if ONEUI_DEVELOPER_TOOLS
    if(diagnostics.size()%256==0) {
        for(auto it=diagnostics.begin();it!=diagnostics.end();) {
            if(it->second.widget.expired())it=diagnostics.erase(it);else ++it;
        }
    }
    diagnostics[widget.get()]={widget,node,box};
#endif
}
std::vector<StyleDiagnostic> styleDiagnostics() {
    std::vector<StyleDiagnostic> result;
#if ONEUI_DEVELOPER_TOOLS
    for(auto it=diagnostics.begin();it!=diagnostics.end();) {
        if(it->second.widget.expired())it=diagnostics.erase(it);
        else {result.push_back(it->second);++it;}
    }
#endif
    return result;
}
}
