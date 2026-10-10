#include "oneui/ui_declarative.h"
namespace oneui::ui {
void Mount::set(Element &e, const std::string &key, const std::wstring &value) {
    if(e.component=="FontPicker" && key=="trigger-label") {e.as<FontPicker>()->setTriggerLabel(value);return;}
    if(e.component=="MediaPicker") {
        if(key=="choose-label"){e.as<MediaPicker>()->setChooseLabel(value);return;}
        if(key=="clear-label"){e.as<MediaPicker>()->setClearLabel(value);return;}
        if(key=="cancel-label"){e.as<MediaPicker>()->setCancelLabel(value);return;}
    }
    if (key == "icon" || key == "symbol") {
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
        } else if (auto w = e.as<Switch>())
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
    if (key == "visible")
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
    else if (key == "checked")
        e.as<Switch>()->setChecked(value);
    else
        throw std::invalid_argument("Invalid bool property");
}
void Mount::set(Element &e, const std::string &key, float value) {
    syntax::validateLayoutNumber(key, value);
    if (key == "content-height") { e.as<ScrollView>()->setContentHeight(value); return; }
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
}
void Mount::set(Element &e, const std::string &key, int value) {
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
