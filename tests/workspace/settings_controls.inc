// Included in the picker suite's anonymous namespace; exercises native adapters.
void settingsControls() {
    check(ui::namedIcon("key-dots") == IconSymbol::KeyDots &&
          ui::namedIcon("check-circle") == IconSymbol::CheckCircle &&
          ui::namedIcon("notebook") == IconSymbol::Notebook &&
          ui::namedIcon("trash") == IconSymbol::Trash &&
          ui::namedIcon("user") == IconSymbol::User &&
          ui::namedIcon("refresh-circle") == IconSymbol::Refresh &&
          ui::namedIcon("edit") == IconSymbol::Edit,
          "security templates retain the existing policy icon symbols");
    ui::Mount mount({}, std::make_shared<ui::StyleRegistry>(), std::make_shared<MemoryClipboard>());
    auto segmented = mount.make("SegmentedControl");
    mount.set(segmented, "items", std::vector<std::wstring>{L"中文", L"English"});
    mount.set(segmented, "selectedIndex", 1);
    check(segmented.as<Tabs>() && segmented.as<Tabs>()->selectedIndex() == 1,
          "segmented selection uses the native Tabs control");
    auto input = mount.make("Input");
    mount.set(input, "password", true);
    mount.set(input, "read-only", true);
    check(input.as<TextField>()->passwordMode() && input.as<TextField>()->readOnly(),
          "declarative password and readonly preserve native input behavior");
    auto text = mount.make("Text");
    mount.set(text, "wrap", true);
    mount.set(text, "max-lines", 0);
    bool rejected = false;
    try { mount.set(text, "max-lines", -1); } catch (const std::invalid_argument &) { rejected = true; }
    check(rejected, "negative line limits are rejected");
    auto scroll = mount.make("Scroll", {text});
    mount.set(scroll, "content-height", 800.0f);
    mount.set(scroll, "wheel-step", 48.0f);
    mount.set(scroll, "chrome-visible", false);
    scroll.widget->setFrame({0, 0, 300, 200});
    mount.set(scroll, "scroll-offset", 75.0f);
    check(scroll.as<ScrollView>()->scrollOffset() == 75.0f, "authored scroll focus offset is retained");
    auto fixed = mount.make("Button", {}, "fixed");
    auto row = mount.make("Row", {fixed});
    mount.styles()->replace(ui::syntax::css("Button.fixed { width:132px; height:32px; }"));
    row.widget->setFrame({0, 0, 500, 58});
    test_support::RecordingCanvas arranged;
    row.widget->paint(arranged);
    check(fixed.widget->frame().width == 132 && fixed.widget->frame().height == 32,
          "explicit CSS dimensions override native button measurement");
#if ONEUI_DEVELOPER_TOOLS
    {
        const auto diagnostics=ui::styleDiagnostics();
        const auto found=std::find_if(diagnostics.begin(),diagnostics.end(),[&](const auto& item){return item.widget.lock()==fixed.widget;});
        check(found!=diagnostics.end() && found->node.tag=="button" &&
              std::find(found->node.classes.begin(),found->node.classes.end(),"fixed")!=found->node.classes.end() &&
              found->box.width==132.f, "declarative diagnostics expose authored classes and resolved dimensions");
        const Widget* retired=nullptr;
        {auto temporary=mount.make("Text");retired=temporary.widget.get();}
        const auto after=ui::styleDiagnostics();
        check(std::none_of(after.begin(),after.end(),[&](const auto& item){return item.widget.lock().get()==retired;}),
              "diagnostics never keep a retired widget alive");
    }
#endif
    auto choice = mount.make("Select", {}, "fixed-choice");
    auto slot = mount.make("Column", {choice});
    mount.styles()->replace(ui::syntax::css("Select.fixed-choice { width:138px; height:32px; }"));
    slot.widget->setFrame({0, 0, 138, 44});
    slot.widget->paint(arranged);
    check(choice.widget->frame().height == 32, "Select width preference never becomes a slot height");
    {
        auto transient=std::make_shared<StyleSheet>();
        std::string error;
        check(transient->addRulesFromCss("select.fixed-choice {background:#f6f7f8;content-background-color:#fcfcfd;color:#182028;}",&error), "transient select CSS");
        mount.styles()->inheritTransient(transient);
        mount.styles()->replace(ui::syntax::css("Select.fixed-choice {width:138px;height:32px;background-color:#182028;}"));
        choice.as<Select>()->setItems({L"First",L"Second"});
        choice.widget->onMouseDown({{20,16}});
        choice.widget->onMouseUp({{20,16}});
        test_support::RecordingCanvas popup;
        choice.widget->paint(popup);
        check(std::any_of(popup.fillRects.begin(),popup.fillRects.end(),[](const auto& r){return r.color.r==252&&r.color.g==252&&r.color.b==253;}),
              "declarative Select retains the host transient palette across CSS replacement");
        choice.widget->onFocusChanged(false);
    }
    auto font = mount.make("FontPicker");
    mount.set(font, "trigger-label", L"Browse fonts");
    auto trigger = std::dynamic_pointer_cast<Button>(font.as<FontPicker>()->children().front());
    check(trigger && trigger->text() == L"Browse fonts", "localized font trigger");
    FontPickerConfig fontConfig;
    fontConfig.items = {{"mono", L"Consolas", L"Consolas", true}};
    fontConfig.selected = "mono";
    font.as<FontPicker>()->setConfig(fontConfig);
    check(trigger->text() == L"Browse fonts" &&
          font.widget->accessibilityInfo().value == L"Consolas",
          "font config retains localized trigger and exposes selected family");
    auto media = mount.make("MediaPicker");
    mount.set(media, "choose-label", L"Choose image");
    mount.set(media, "clear-label", L"Clear image");
    mount.set(media, "cancel-label", L"Cancel load");
    media.widget->setFrame({0, 0, 330, 32});
    media.widget->paint(arranged);
    std::vector<std::wstring> mediaLabels;
    for (auto& child : media.as<MediaPicker>()->children())
        if (auto button = std::dynamic_pointer_cast<Button>(child))
            mediaLabels.push_back(button->text());
    check(mediaLabels == std::vector<std::wstring>{L"Choose image", L"Clear image", L"Cancel load"},
          "media action labels remain independently localizable");
    mount.styles()->replace(ui::syntax::css(
        "Button.media-picker-choose { background-color:#26384a; color:#f0f1f2; }"
        "Button.media-picker-clear:disabled { background-color:#19212a; color:#8a8b8c; }"));
    test_support::RecordingCanvas mediaPaint;
    media.widget->tickAnimations(1e12);
    media.widget->paint(mediaPaint);
    bool choosePainted=false, clearPainted=false;
    for (const auto& fill:mediaPaint.fillRects) {
        choosePainted |= fill.color.r==0x26 && fill.color.g==0x38;
        clearPainted |= fill.color.r==0x19 && fill.color.g==0x21;
    }
    check(choosePainted && clearPainted, "media actions consume independent dark/disabled styles");
    {
        internal::ScopedUiClock clock([]{return 100.;});
        TextField field;
        field.setFrame({0,0,200,32});
        field.onFocusChanged(true);
        internal::freezeCarets=true;
        field.tickAnimations(800.);
        test_support::RecordingCanvas first,second;
        field.paint(first);
        field.tickAnimations(1200.);
        field.paint(second);
        internal::freezeCarets=false;
        check(first.fillRects.size()==second.fillRects.size(), "capture caret stays visible across blink phases");
    }
    auto card = mount.make("InteractiveSurface", {text});
    int activated = 0;
    ui::VmCommand command;
    command.setAction([&] { ++activated; });
    mount.click(card, command);
    card.widget->setFrame({0, 0, 168, 112});
    card.widget->onMouseDown({{20, 20}, MouseButton::Left});
    card.widget->onMouseUp({{20, 20}, MouseButton::Left});
    check(activated == 1, "declarative interactive surface invokes native click");
    mount.styles()->replace(ui::syntax::css(
        "Input { font-size:19px; }"
        "SegmentedControl { background-color:#18201b; content-background-color:#18201b; color:#e3eae5; }"
        "SegmentedControl:selected { content-background-color:#35453b; }"
        "Switch:checked { background-color:#4169aa; }"));
    check(input.as<TextField>()->fontSize() == 19, "Input consumes authored font size");
    segmented.widget->setFrame({0, 0, 300, 34});
    test_support::RecordingCanvas styled;
    segmented.widget->paint(styled);
    check(!styled.fillRects.empty() && styled.fillRects.front().color.r == 24,
          "SegmentedControl consumes authored dark background");
    bool selectedPainted = false;
    for (const auto& fill : styled.fillRects) selectedPainted |= fill.color.r == 53;
    check(selectedPainted, "SegmentedControl consumes selected item background");
    mount.styles()->replace("");
    check(input.as<TextField>()->fontSize() == 14, "removing input font style restores default");
}
