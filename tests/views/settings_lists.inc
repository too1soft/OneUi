#include "../support/recording_canvas.h"
// Native list composition, VM events and record notifications through real IR.
void settingsLists(const char* runtimeFile) {
    const auto path = std::filesystem::path(runtimeFile).parent_path() / "Lists.oneview";
    Case c(path.string().c_str());
    c.a.text(id(c.image, "Item", "title"), "Alpha");
    c.a.text(id(c.image, "Item", "detail"), "First action");
    c.b.text(id(c.image, "Item", "title"), "Beta");
    c.b.text(id(c.image, "Item", "detail"), "Second action");
    const auto height = id(c.image, "Root", "height");
    c.vm.values[height].number = 96;
    Vm span(c.image, "Span");
    span.text(span.key, "prompt");
    span.values[id(c.image, "Span", "start")].integer = 0;
    span.values[id(c.image, "Span", "end")].integer = 2;
    span.values[id(c.image, "Span", "rgba")].integer = 0x123456ff;
    span.values[id(c.image, "Span", "monospace")].integer = 1;
    const auto spans = id(c.image, "Root", "spans");
    c.vm.lists[spans] = {&span};
    c.start();
    auto flow = c.runtime->root->children[2]->children[0]->element.as<oneui::Stack>();
    auto surfaceFlow = c.runtime->root->children[3]->children[0]->element.as<oneui::Stack>();
    expect(flow->direction() == oneui::StackDirection::Row && flow->gap() == 12,
           "repeat retains parent row direction and spacing");
    expect(surfaceFlow->gap() == 8, "repeat inherits surface content spacing");
    c.runtime->root->element.widget->setFrame({0, 0, 600, 900});
    oneui::test_support::RecordingCanvas canvas;
    c.runtime->root->element.widget->paint(canvas);
    auto surface = c.runtime->root->children[3]->element.widget;
    expect(surface->frame().height == 96, "initial bound basis reaches parent flex");
    c.vm.values[height].number = 128;
    c.runtime->notify(c.runtime->rootScope, height);
    c.vm.text(c.title, "changed-layout");
    c.runtime->notify(c.runtime->rootScope, c.title);
    c.runtime->flush();
    c.runtime->root->element.widget->paint(canvas);
    expect(surface->frame().height == 128, "basis notification updates parent layout");
    expect(c.runtime->root->children[4]->element.classes.find("changed-layout") != std::string::npos,
           "Column class binding updates without losing scope");
    auto action = c.runtime->root->children[5];
    c.runtime->invoke(action, action->events.front(), {});
    expect(c.vm.invokes == 1, "InteractiveSurface invokes generated command");
    auto rich = c.runtime->root->children[6]->element.as<oneui::Label>();
    expect(rich->text() == L"changed-layout" && rich->textSpans().size() == 1 &&
           rich->textSpans()[0].monospace && rich->textSpans()[0].foreground.r == 0x12,
           "rich text notification retains spans after text changes");
    auto list = c.runtime->root->children[0]->element.as<oneui::List>();
    auto virtualList = c.runtime->root->children[1]->element.as<oneui::VirtualList>();
    expect(list->items().size() == 2 && list->items()[0].detail == L"First action",
           "List receives title/detail records");
    expect(virtualList->rowHeight() == 42 && virtualList->items()[1].title == L"Beta",
           "VirtualList receives native row height and records");
    virtualList->setFrame({0, 0, 300, 100});
    oneui::KeyEvent down;
    down.key = oneui::Key::Down;
    virtualList->onKeyDown(down);
    expect(c.vm.values[id(c.image, "Root", "selected")].integer == 1,
           "native keyboard selection writes the VM");
    c.runtime->notify(c.runtime->rootScope, id(c.image, "Root", "selected"));
    c.runtime->flush();
    expect(list->selectedIndex() == 1, "selection notifications update other consumers");
    c.a.text(id(c.image, "Item", "detail"), "Changed action");
    c.runtime->notify(c.runtime->list(c.runtime->rootScope, c.rows).items[0],
                      id(c.image, "Item", "detail"));
    c.runtime->flush();
    expect(virtualList->items()[0].detail == L"Changed action", "record change refreshes native row");
    c.vm.items = {&c.b, &c.a};
    ++c.vm.revision;
    c.runtime->notify(c.runtime->rootScope, c.rows);
    c.runtime->flush();
    expect(list->items()[0].title == L"Beta" && virtualList->items()[1].title == L"Alpha",
           "list reset updates order for both controls");
    const auto spanStart = id(c.image, "Span", "start");
    const auto spanEnd = id(c.image, "Span", "end");
    c.vm.text(c.title, "\xe4\xb8\xad\xf0\x9f\x9a\x80");
    span.values[spanStart].integer = 3;
    span.values[spanEnd].integer = 7;
    c.runtime->notify(c.runtime->rootScope, c.title);
    c.runtime->notify(c.runtime->rootScope, spans);
    c.runtime->flush();
    expect(rich->textSpans()[0].start == 1 && rich->textSpans()[0].end == 3,
           "UTF8 byte spans convert supplementary characters to native offsets");
    for (const auto range : {std::pair<int,int>{1,7}, {3,8}, {3,3}}) {
        span.values[spanStart].integer = range.first;
        span.values[spanEnd].integer = range.second;
        c.runtime->notify(c.runtime->rootScope, spans);
        try {
            c.runtime->flush();
            expect(false, "invalid rich text range accepted");
        } catch (const Failure& error) {
            expect(error.result == ONEUI_VIEW_TYPE && rich->textSpans()[0].start == 1,
                   "invalid UTF8 boundary/range preserves prior rich text");
        }
    }
    c.runtime->close();
    expect(c.a.retains == c.a.releases && c.b.retains == c.b.releases,
           "list record subscriptions release with view");
}
