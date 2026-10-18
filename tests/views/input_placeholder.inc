// Input and SearchInput share TextField; both accept the focused-placeholder flag.
void inputPlaceholder() {
    for(const char* type:{"Input","SearchInput"}) {
        oneui::ui::Mount factory({},std::make_shared<oneui::ui::StyleRegistry>(),
                                 std::make_shared<oneui::MemoryClipboard>());
        auto element=factory.make(type);
        auto field=element.as<oneui::TextField>();
        expect(!field->placeholderVisibleWhenFocused(),"focused placeholder defaults off");
        factory.set(element,"placeholder-when-focused",true);
        expect(field->placeholderVisibleWhenFocused(),"placeholder-when-focused reaches TextField");
    }
    const auto& input=oneview::registry().at("Input");
    expect(input.property("placeholder-when-focused").id==18,"Input appends placeholder-when-focused as ID 18");
}
