#include "oneui/ui_declarative.h"
#include "data_menu.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/data_grid.h"
#include "oneui/controls/tree_view.h"
#include "oneui/controls/code_editor.h"
#include "internal/unicode.h"
namespace oneui::ui {
void Mount::set(Element &e, const std::string &key, const std::wstring &value) {
    if (e.component == "CodeEditor") {
        auto editor = e.as<CodeEditor>();
        if (key == "text") { editor->setBoundText(value); return; }
        if (key == "comment-prefix") { editor->setCommentPrefix(value); return; }
        if (key == "language") { editor->setLanguage(parseCodeLanguage(unicode::toUtf8(value))); return; }
    }
    if (e.component == "TreeView") {
        if (key == "selected-id") { e.as<TreeView>()->setSelectedId(value); return; }
        if (key == "loading-text") { e.as<TreeView>()->setLoadingText(value); return; }
    }
    if(e.component=="Popup" && key=="placement") {
        const std::map<std::wstring,PopupPreferredPlacement> placements{
            {L"bottom-start",PopupPreferredPlacement::BottomStart}, {L"bottom-end",PopupPreferredPlacement::BottomEnd},
            {L"top-start",PopupPreferredPlacement::TopStart}, {L"top-end",PopupPreferredPlacement::TopEnd},
            {L"left-start",PopupPreferredPlacement::LeftStart}, {L"right-start",PopupPreferredPlacement::RightStart}};
        const auto found=placements.find(value);
        if(found==placements.end())throw std::invalid_argument("Unknown popup placement");
        e.as<Popup>()->setPreferredPlacement(found->second); return;
    }
    if(e.component=="DataMenu") {
        if(key=="title") { e.as<views::DataMenu>()->setTitle(value); return; }
        if(key=="subtitle") { e.as<views::DataMenu>()->setSubtitle(value); return; }
    }
    if(e.component=="GridItem" && key=="grid-key") {e.as<TemplateGridItem>()->setKey(value);return;}
    if (e.component == "Overlay" && (key == "horizontal-align" || key == "vertical-align")) {
        e.as<TemplateOverlay>()->setAlignment(key, value); return;
    }
    if (key == "prefix-icon" || key == "trailing-icon") {
        const auto symbol = value.empty() ? std::optional<IconSymbol>{} : namedIcon(std::string(value.begin(), value.end()));
        if (!value.empty() && !symbol) throw std::invalid_argument("Unknown authoring icon");
        if (key == "prefix-icon") e.as<TextField>()->setPrefixIcon(symbol);
        else if (symbol) e.as<Button>()->setTrailingIcon(*symbol);
        else e.as<Button>()->clearTrailingIcon();
        return;
    }
    if (key == "trailing-text") { e.as<Button>()->setTrailingText(value); return; }
    if (key == "tooltip") { e.widget->setTooltip(value); return; }
    if (key == "text-align" || key == "content-align") {
        if (value != L"start" && value != L"center" && value != L"end")
            throw std::invalid_argument("text/content-align must be start, center or end");
        const auto align = value == L"center" ? TextAlign::Center :
                           value == L"end" ? TextAlign::Right : TextAlign::Left;
        if (key == "text-align") e.as<Label>()->setAlign(align);
        else e.as<Button>()->setContentAlign(align);
        return;
    }
    if(e.component=="Dialog") {
        auto dialog=e.as<Dialog>();
        if(key=="title"){dialog->setTitle(value);return;}
        if(key=="subtitle"){dialog->setSubtitle(value);return;}
        if(key=="icon") {
            if(value.empty())dialog->clearIconSymbol();
            else {
                const auto symbol=namedIcon(std::string(value.begin(),value.end()));
                if(!symbol)throw std::invalid_argument("Unknown authoring icon");
                dialog->setIconSymbol(*symbol);
            }
            return;
        }
    }
    if(e.component=="FontPicker" && key=="trigger-label") {e.as<FontPicker>()->setTriggerLabel(value);return;}
    if(e.component=="MediaPicker") {
        if(key=="choose-label"){e.as<MediaPicker>()->setChooseLabel(value);return;}
        if(key=="clear-label"){e.as<MediaPicker>()->setClearLabel(value);return;}
        if(key=="cancel-label"){e.as<MediaPicker>()->setCancelLabel(value);return;}
    }
    if (key == "password-mask") {
        if(value.size()!=1 || (value[0]>=0xd800 && value[0]<=0xdfff))
            throw std::invalid_argument("password-mask requires one BMP character");
        e.as<TextField>()->setPasswordMask(value[0]);
    } else if (key == "icon" || key == "symbol") {
        if (value.empty()) {
            if (key == "icon") e.as<Button>()->clearIcon();
            else e.as<IconView>()->setSymbol(IconSymbol::RemoteAssist);
            return;
        }
        const auto symbol = namedIcon(std::string(value.begin(), value.end()));
        if (!symbol)
            throw std::invalid_argument("Unknown authoring icon");
        if (key == "icon")
            e.as<Button>()->setIcon(*symbol);
        else
            e.as<IconView>()->setSymbol(*symbol);
    } else if (key == "presentation") {
        if (value != L"document" && value != L"segmented")
            throw std::invalid_argument("Tabs presentation must be document or segmented");
        auto tabs = e.as<Tabs>();
        tabs->setDocumentMode(value == L"document");
        tabs->setSizingMode(value == L"document" ? TabsSizingMode::Compact : TabsSizingMode::Equal);
    } else if (key == "orientation") {
        if (value != L"horizontal" && value != L"vertical")
            throw std::invalid_argument("orientation must be horizontal or vertical");
        e.as<SplitView>()->setOrientation(value == L"horizontal" ? SplitOrientation::Horizontal
                                                                 : SplitOrientation::Vertical);
    } else if (key == "align") {
        if (e.content) {
            if (value != L"start" && value != L"center" && value != L"end")
                throw std::invalid_argument("Content align must be start, center or end");
            e.as<Stack>()->setJustify(value == L"center" ? StackJustify::Center
                                      : value == L"end"  ? StackJustify::End
                                                         : StackJustify::Start);
        } else if (e.component == "Row" || e.component == "Column") {
            if (value != L"start" && value != L"center" && value != L"end" && value != L"stretch")
                throw std::invalid_argument("align must be start, center, end or stretch");
            e.as<Stack>()->setAlign(value == L"start"    ? StackAlign::Start
                                    : value == L"center" ? StackAlign::Center
                                    : value == L"end"    ? StackAlign::End
                                                         : StackAlign::Stretch);
        } else
            throw std::invalid_argument("align requires Row, Column or Content");
    } else if (key == "justify") {
        if (e.component != "Row" && e.component != "Column")
            throw std::invalid_argument("justify requires Row or Column");
        if (value != L"start" && value != L"center" && value != L"end" && value != L"space-between")
            throw std::invalid_argument("justify must be start, center, end or space-between");
        e.as<Stack>()->setJustify(value == L"center"          ? StackJustify::Center
                                  : value == L"end"           ? StackJustify::End
                                  : value == L"space-between" ? StackJustify::SpaceBetween
                                                              : StackJustify::Start);
    } else if (key == "variant")
        styles_->variant(e, value);
    else if (key == "appearance")
        styles_->appearance(e, value);
    else if (key == "tone") {
        if (e.component != "Status")
            throw std::invalid_argument("tone requires Status");
        styles_->tone(e, value);
    } else if (key == "subtitle") {
        if (!e.hint)
            throw std::invalid_argument("Component has no subtitle");
        e.hint->setText(value);
        e.hint->setVisible(!value.empty());
    } else if (key == "error") {
        if (!e.error || !e.fieldControl)
            throw std::invalid_argument("error requires FormRow");
        e.error->setText(value);
        e.error->setVisible(!value.empty());
        styles_->invalid(e.fieldControl, !value.empty());
        e.fieldControl->setAccessibleDescription(e.hint->text() +
                                                 (value.empty() ? L"" : L" " + value));
    } else if (key == "name") {
        e.widget->setAccessibleName(value);
        if (e.component == "ToolButton")
            e.widget->setTooltip(value);
    } else if (key == "description")
        e.widget->setAccessibleDescription(value);
    else if (key == "title" || key == "label") {
        if (!e.title)
            throw std::invalid_argument("Component has no title");
        e.title->setText(value);
        e.title->setVisible(!value.empty());
        if (key == "label" && e.fieldControl)
            e.fieldControl->setAccessibleName(value);
    } else if (key == "hint") {
        e.hint->setText(value);
        e.hint->setVisible(!value.empty());
        if (e.fieldControl)
            e.fieldControl->setAccessibleDescription(
                value + (e.error && !e.error->text().empty() ? L" " + e.error->text() : L""));
    } else if (key == "preset") {
        if (value != L"fade" && value != L"expand")
            throw std::invalid_argument("Reveal preset must be fade or expand");
        e.as<Reveal>()->setPreset(value == L"fade" ? RevealPreset::Fade : RevealPreset::Expand);
    } else if (key == "placeholder")
        e.as<TextField>()->setPlaceholder(value);
    else if (key == "selectedKey") {
        if (e.tabs)
            e.tabs->select(value);
        else
            e.table->select(value);
    } else if (key == "text") {
        if (auto w = e.as<Label>())
            w->setText(value);
        else if (auto w = e.as<Button>()) {
            w->setText(value);
            if (e.component == "ToolButton")
                w->setPreferredSize({value.empty() ? 32.0f : 0.0f, 32});
        } else if (auto w = e.as<Checkbox>())
            w->setText(value);
        else if (auto w = e.as<Switch>())
            w->setText(value);
        else if (auto w = e.as<TextField>()) {
            if (w->text() != value)
                w->setText(value);
        }
        if (e.component == "ValidationMessage")
            e.widget->setVisible(!value.empty());
    } else
        throw std::invalid_argument("Invalid text property: " + key);
}
void Mount::set(Element &e, const std::string &key, const wchar_t *value) {
    set(e, key, std::wstring(value));
}
void Mount::set(Element &e, const std::string &key, const std::string &value) {
    if (key == "class") {
        e.classes += " " + value;
        styles_->add(e);
    } else
        set(e, key, wide(value));
}
void Mount::set(Element &e, const std::string &key, bool value) {
    if (e.component == "CodeEditor") {
        auto editor = e.as<CodeEditor>();
        if (key == "read-only") { editor->setReadOnly(value); return; }
        if (key == "word-wrap") { editor->setWordWrap(value); return; }
        if (key == "line-numbers") { editor->setLineNumbers(value); return; }
        if (key == "indent-guides") { editor->setIndentGuides(value); return; }
        if (key == "insert-spaces") { editor->setInsertSpaces(value); return; }
        if (key == "completion-open") { editor->setCompletionOpen(value); return; }
    }
    if (e.component == "DataGrid") {
        if (key == "row-numbers") { e.as<DataGrid>()->setRowNumbersVisible(value); return; }
        if (key == "editable") { e.as<DataGrid>()->setEditable(value); return; }
    }
    if(e.component=="Popup" && key=="open") {e.as<Popup>()->setOpen(value);return;}
    if(key=="placeholder-when-focused") {e.as<TextField>()->setPlaceholderVisibleWhenFocused(value);return;}
    if(e.component=="ReorderableGrid") {
        if(key=="reorder-enabled"){e.as<ReorderableGrid>()->setReorderEnabled(value);return;}
        if(key=="item-drag-enabled"){e.as<ReorderableGrid>()->setItemDragEnabled(value);return;}
    }
    if (key == "overlay-visible")
        e.as<TemplateOverlay>()->setOverlayVisible(value);
    else if (key == "close-visible")
        e.as<Dialog>()->setCloseVisible(value);
    else if (key == "visible")
        e.widget->setVisible(value);
    else if (key == "disabled")
        e.widget->setDisabled(value);
    else if (key == "password")
        e.as<TextField>()->setPasswordMode(value);
    else if (key == "read-only")
        e.as<TextField>()->setReadOnly(value);
    else if (key == "chrome-visible")
        e.as<ScrollView>()->setChromeVisible(value);
    else if (key == "collapsed") {
        for (auto &part : e.collapsible)
            part->setVisible(!value);
    } else if (key == "second-collapsed")
        e.as<SplitView>()->setSecondCollapsed(value);
    else if (key == "closable")
        e.as<Tabs>()->setClosable(value);
    else if (key == "themed") {
        e.as<NativeHostView>()->setThemed(value);
        styles_->add(e);
    } else if (key == "wrap") {
        if (auto label = e.as<Label>()) { label->setTextWrapping(value); return; }
        if (e.component != "Row" && e.component != "Column")
            throw std::invalid_argument("wrap requires Row or Column");
        e.as<Stack>()->setWrap(value);
    } else if (key == "detail-open")
        e.as<AdaptivePanes>()->setDetailOpen(value);
    else if (key == "open")
        e.as<Reveal>()->setOpen(value);
    else if (key == "reduced-motion")
        e.as<Reveal>()->setReducedMotion(value);
    else if (key == "checked") {
        if(auto checkbox=e.as<Checkbox>())checkbox->setChecked(value);
        else e.as<Switch>()->setChecked(value);
    }
    else
        throw std::invalid_argument("Invalid bool property");
}
void Mount::set(Element &e, const std::string &key, float value) {
    if (e.component == "DataGrid") {
        if (key == "row-height") { e.as<DataGrid>()->setRowHeight(value); return; }
        if (key == "header-height") { e.as<DataGrid>()->setHeaderHeight(value); return; }
    }
    if (e.component == "TreeView" && key == "row-height") { e.as<TreeView>()->setRowHeight(value); return; }
    if (e.component == "CodeEditor" && key == "font-size") { e.as<CodeEditor>()->setFontSize(value); return; }
    syntax::validateLayoutNumber(key, value);
    if(e.component=="Popup") {
        auto popup=e.as<Popup>();
        if(key.rfind("anchor-",0)==0) {
            auto rect=popup->anchorRect().value_or(Rect{});
            if(key=="anchor-x")rect.x=value;
            else if(key=="anchor-y")rect.y=value;
            else if(key=="anchor-width")rect.width=std::max(0.0f,value);
            else if(key=="anchor-height")rect.height=std::max(0.0f,value);
            else throw std::invalid_argument("Unknown popup anchor property");
            popup->setAnchorRect(rect);return;
        }
        if(key=="content-width"||key=="content-height") {
            auto size=popup->content()->preferredSize();
            if(key=="content-width")size.width=std::max(1.0f,value);else size.height=std::max(1.0f,value);
            popup->content()->setPreferredSize(size);return;
        }
    }
    if(e.component=="ReorderableGrid") {
        if(key=="column-gap"){e.as<ReorderableGrid>()->setColumnGap(value);return;}
        if(key=="row-gap"){e.as<ReorderableGrid>()->setRowGap(value);return;}
        if(key=="item-height"){e.as<ReorderableGrid>()->setItemHeight(value);return;}
    }
    if (e.component == "Overlay" && (key.rfind("overlay-",0)==0 || key.rfind("inset-",0)==0)) {
        e.as<TemplateOverlay>()->setNumber(key,value); return;
    }
    if (auto table = e.as<TemplateTable>()) {
        if (key == "minimum-width") { table->setMinimumWidth(value); return; }
        if (key == "header-height") { table->setHeaderHeight(value); return; }
          if (key == "content-height") { table->setContentHeight(value); return; }
          if (key == "content-width") { table->setContentWidth(value); return; }
        if (key == "wheel-step") { table->setWheelStep(value); return; }
        if (key == "scroll-offset") { table->setScrollOffset(value); return; }
        if (key == "horizontal-offset") { table->setHorizontalScrollOffset(value); return; }
    }
    if (key == "line-height") {
        if (auto label = e.as<Label>()) label->setLineHeight(value);
        else e.as<TextField>()->setLineHeight(value);
        return;
    }
    if (key == "content-height") { e.as<ScrollView>()->setContentHeight(value); return; }
    if (key == "content-width") { e.as<ScrollView>()->setContentWidth(value); return; }
    if (key == "wheel-step") { e.as<ScrollView>()->setWheelStep(value); return; }
    if (key == "scroll-offset") {
        if (e.component == "VirtualList") e.as<VirtualList>()->setScrollOffset(value);
        else e.as<ScrollView>()->setScrollOffset(value);
        return;
    }
    if (key == "row-height") { e.as<VirtualList>()->setRowHeight(value); return; }
    if (key == "value") {
        e.as<ProgressBar>()->setValue(std::clamp(value, 0.0f, 1.0f));
        return;
    }
    if (key == "collapsed-extent") {
        e.as<SplitView>()->setCollapsedExtent(value);
        return;
    }
    if (key == "size") {
        e.widget->setPreferredSize({value, value});
        return;
    }
    if (key == "ratio") {
        e.as<SplitView>()->setSplitRatio(value);
        return;
    }
    if (key == "first-min" || key == "second-min") {
        if (key == "first-min")
            e.splitLimits->first = value;
        else
            e.splitLimits->second = value;
        e.as<SplitView>()->setMinimumPaneExtent(e.splitLimits->first, e.splitLimits->second);
        return;
    }
    if (key == "breakpoint") {
        e.as<AdaptivePanes>()->setBreakpoint(value);
        return;
    }
    if (key == "pane-width") {
        e.as<AdaptivePanes>()->setPaneWidth(value);
        return;
    }
    if (key == "min-column-width") {
        if (e.component != "FormGrid")
            throw std::invalid_argument("min-column-width requires FormGrid");
        for (auto &child : e.as<Stack>()->children())
            e.as<Stack>()->setFlex(child, {1, 1, value, 0});
        return;
    }
    if (key == "max-width") {
        if (!e.content)
            throw std::invalid_argument("max-width is only supported on Content");
        e.as<Stack>()->setFlex(e.content, {1, 1, value, 0, value, true});
        return;
    }
    if (key == "grow")
        e.grow(value);
    else if (key == "basis")
        e.basis(value);
    else if (key == "min")
        e.min(value);
    else if (key == "max")
        e.max(value);
    else if (key == "shrink")
        e.shrink(value);
    else
        throw std::invalid_argument("Invalid layout property");
    styles_->updateAuthoredFlex(e.widget,e.flex);
}
void Mount::set(Element &e, const std::string &key, int value) {
    if (e.component == "CodeEditor") {
        if (key == "text-revision") {
            if (value < 0) throw std::invalid_argument("text-revision must not be negative");
            e.as<CodeEditor>()->setBoundRevision(static_cast<std::uint64_t>(value));
            return;
        }
        if (key == "tab-size") { e.as<CodeEditor>()->setTabSize(value); return; }
        if (key == "decorations-revision") {
            if (value < 0) throw std::invalid_argument("decorations-revision must not be negative");
            e.as<CodeEditor>()->setBoundDecorationsRevision(static_cast<std::uint64_t>(value));
            return;
        }
    }
    if (e.component == "DataGrid") {
        auto grid = e.as<DataGrid>();
        if (key == "row-count") { grid->setRowCount(value); return; }
        if (key == "window-start") { grid->setWindowStart(value); return; }
        if (key == "frozen-columns") { grid->setFrozenColumns(value); return; }
        // Selection fields arrive one at a time; each refines the current selection.
        auto selection = grid->selection();
        if (key == "anchor-row") selection.anchorRow = value;
        else if (key == "anchor-column") selection.anchorColumn = value;
        else if (key == "focus-row") selection.focusRow = value;
        else if (key == "focus-column") selection.focusColumn = value;
        else if (key == "select-kind") {
            if (value < 0 || value > 3) throw std::invalid_argument("select-kind must be 0-3");
            selection.kind = DataGridSelectKind(value);
        } else throw std::invalid_argument("Unknown DataGrid integer property");
        grid->setSelection(selection);
        return;
    }
    if(e.component=="ReorderableGrid" && key=="column-count") {
        if(value<1)throw std::invalid_argument("column-count must be positive");
        e.as<ReorderableGrid>()->setColumnCount(value);return;
    }
    if (key == "max-lines") {
        if (value < 0) throw std::invalid_argument("max-lines must be nonnegative");
        e.as<Label>()->setMaxLines(value);
    }
    else if (key == "selectedIndex") {
        if (e.component == "List") e.as<List>()->setSelectedIndex(value);
        else if (e.component == "VirtualList") e.as<VirtualList>()->setSelectedIndex(value);
        else if (e.component == "SegmentedControl") e.as<Tabs>()->setSelectedIndex(value);
        else e.as<Select>()->setSelectedIndex(value);
    }
    else
        set(e, key, float(value));
}
void Mount::set(Element &e, const std::string &, const std::vector<std::wstring> &items) {
    if (e.component == "SegmentedControl") e.as<Tabs>()->setItems(items);
    else e.as<Select>()->setItems(items);
}
void Mount::set(Element &e, const std::string &, const std::vector<ListItem> &items) {
    if (e.component == "List") e.as<List>()->setItems(items);
    else e.as<VirtualList>()->setItems(items);
}
void Mount::set(Element &e, const std::string &, const std::vector<TableColumn> &columns) {
    e.as<Table>()->setColumns(columns);
}
void Mount::set(Element &e, const std::string &, const std::vector<TableRow> &rows) {
    e.table->update(rows);
}
void Mount::set(Element &e, const std::string &, const std::vector<TabItem> &items) {
    e.tabs->update(items);
}
void Mount::set(Element &e, const std::string &, const std::vector<TimeSeriesChartSeries> &series) {
    e.as<TimeSeriesChart>()->setSeries(series);
}
} // namespace oneui::ui
