// A slotted container popup takes its declared content size; children that
// grow from a zero basis must fill it rather than collapse to their minimums.
void popupSurface(const char* runtimeFile) {
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"PopupSurface.oneview";
    const auto image=load(path.string().c_str());
    Vm vm(image,"Root");
    auto runtime=std::make_shared<Runtime>(image);
    runtime->initialize(&vm.table,&vm);
    // Like a product VM: size and open state arrive together after creation.
    for(auto [name,apply]:std::initializer_list<std::pair<const char*,std::function<void(Value&)>>>{
            {"width",[](Value& v){v.number=360;}},{"height",[](Value& v){v.number=300;}},
            {"open",[](Value& v){v.integer=1;}}}) {
        const auto property=id(image,"Root",name);apply(vm.values[property]);
        runtime->notify(runtime->rootScope,property);
    }
    runtime->flush();
    auto root=runtime->root->element.widget;
    oneui::test_support::RecordingCanvas canvas;
    // The popup is a zero-height flow child; placement must use the window viewport.
    canvas.viewportOverride=oneui::Rect{0,0,1000,800};
    root->setFrame({0,0,1000,800});root->paint(canvas);
    // A single-root slot makes the supplied Surface the popup's direct content.
    auto popup=runtime->root->children[0];
    expect(popup->element.component=="Popup" && popup->children[0]->element.component=="Surface",
           "single-root slot omits the wrapper Column");
    auto surface=popup->children[0]->element.widget;
    auto column=popup->children[0]->children[0];
    auto head=column->children[0]->element.widget, body=column->children[1]->element.widget;
    expect(std::abs(surface->frame().width-360)<0.5f && std::abs(surface->frame().height-300)<0.5f,
           "popup surface uses declared content size");
    expect(std::abs(column->element.widget->frame().height-300)<0.5f,"zero-basis child fills popup surface");
    expect(std::abs(head->frame().height-48)<0.5f && std::abs(body->frame().height-252)<0.5f,
           "fixed and growing rows divide the popup height");
}
