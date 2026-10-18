#include "oneui/layout/virtual_template_list.h"
void virtualTemplateContract(const char* runtimeFile) {
    const auto file = std::filesystem::path(runtimeFile).parent_path() / "Lists.oneview";
    std::vector<std::unique_ptr<Vm>> messages;
    Case c(file.string().c_str());
    c.vm.lists[id(c.image, "Root", "spans")] = {};
    // Keep data leases separate from rendered row leases. The existing list
    // protocol snapshots data, while only the viewport connects render scopes.
    c.vm.items.clear();
    const auto textId = id(c.image, "Item", "text");
    for (int i = 0; i < 1000; ++i) {
        auto item = std::make_unique<Vm>(c.image, "Item");
        item->text(item->key, std::to_string(i));
        item->text(textId, "Message " + std::to_string(i));
        item->lists[id(c.image, "Item", "cells")] = {};
        c.vm.items.push_back(item.get());
        messages.push_back(std::move(item));
    }
    // Mount only this compiler-validated subtree: unrelated Lists fixture
    // tables/repeats must not mask the virtual presenter's allocation count.
    auto& image = c.runtime->image;
    const auto outer = image.records[oneview::TMPL][image.root][1];
    image.records[oneview::TMPL][image.root][1] = image.records[oneview::EDGE][image.records[oneview::NODE][outer][3] + 21][1];
    c.start();
    const auto instance = c.runtime->root;
    auto list = instance->element.as<oneui::VirtualTemplateList>();
    expect(list && instance->rows.empty(), "virtual templates wait for a viewport");
    list->setFollowEnd(false);
    list->setScrollOffset(0);
    // Headless: mounting is driven by layout/paint, so request one explicitly.
    list->setFrame({0, 0, 360, 240});
    list->refresh();
    expect(instance->rows.size() <= 20 && instance->rows.size() == list->mountedCount(), "1000 message templates have bounded native instances");
    auto first = instance->rows.at("0");
    const auto firstReads = messages[0]->reads[textId];
    list->setFollowEnd(true);
    list->setScrollOffset(list->maxScrollOffset());
    expect(!instance->rows.count("0") && messages[0]->disconnects == 1, "recycling disconnects render lease");
    const auto last = instance->rows.at("999");
    auto label = last->children[0]->element.as<oneui::Label>();
    auto button = last->children[1]->element.as<oneui::Button>();
    const auto targetReads = messages[999]->reads[textId];
    const auto spansId = id(c.image, "Item", "spans");
    std::vector<std::unique_ptr<Vm>> spanVms;
    auto addSpan = [&](const char* spanKey, int64_t start, int64_t end, int64_t rgba, int64_t mono) {
        auto span = std::make_unique<Vm>(c.image, "Span");
        span->text(span->key, spanKey);
        span->values[id(c.image, "Span", "start")].integer = start;
        span->values[id(c.image, "Span", "end")].integer = end;
        span->values[id(c.image, "Span", "rgba")].integer = rgba;
        span->values[id(c.image, "Span", "monospace")].integer = mono;
        spanVms.push_back(std::move(span));
        return spanVms.back().get();
    };
    // Dark and light rgba spans over multibyte UTF-8 ranges of the new text.
    auto dark = addSpan("s0", 6, 12, 0x1c2b39ff, 0);
    auto light = addSpan("s1", 13, 17, 0xf5f5f5ff, 1);
    dark->values[id(c.image, "Span", "weight")].integer = 650;
    dark->values[id(c.image, "Span", "italic")].integer = 1;
    light->values[id(c.image, "Span", "background")].integer = 0x3b82f61e;
    light->values[id(c.image, "Span", "size")].number = 12.0;
    messages[999]->lists[spansId] = {dark, light};
    messages[999]->text(textId, "UTF-8 \xe4\xb8\xad\xe6\x96\x87 \xf0\x9f\x98\x80\nsecond\nthird\nfourth");
    ++c.vm.revision;
    OneUiUtf8String key{"999", 3};
    OneUiViewListChange update{};
    update.struct_size = sizeof(update); update.kind = ONEUI_LIST_UPDATE;
    update.before_revision = c.vm.revision - 1; update.after_revision = c.vm.revision;
    update.index = 999; update.count = 1; update.keys = &key; update.key_count = 1;
    uint32_t changedProperties[] = {textId, spansId};
    update.properties = changedProperties; update.property_count = 2;
    c.runtime->notifyList(c.runtime->rootScope, c.rows, update);
    c.runtime->flush();
    list->refresh();
    expect(instance->rows.at("999") == last && last->children[1]->element.widget == button, "stream update preserves template and action identity");
    expect(messages[0]->reads[textId] == firstReads && messages[999]->reads[textId] > targetReads, "stream update reads only changed message presentation");
    expect(label->text().find(L'\x4e2d') != std::wstring::npos && list->scrollOffset() == list->maxScrollOffset(), "Unicode stream and end anchoring survive layout");
    const auto& spans = label->textSpans();
    expect(spans.size() == 2 && spans[0].start == 6 && spans[0].end == 8 && spans[0].foreground.r == 0x1c &&
               spans[1].start == 9 && spans[1].end == 11 && spans[1].monospace && spans[1].foreground.g == 0xf5,
           "dark and light Unicode spans reach the streamed row");
    expect(spans[0].fontWeight == 650 && spans[0].italic && !spans[0].underline,
           "optional weight and italic fields reach the first span");
    expect(spans[1].fontSize == 12.0f && spans[1].background.b == 0xf6 && spans[1].background.a == 0x1e,
           "optional size and background fields reach the second span");
    // Paint first: a row's own children are positioned by its layout pass.
    oneui::test_support::RecordingCanvas rowLayout;
    list->paint(rowLayout);
    const auto frame = button->frame();
    expect(frame.width > 0 && frame.height > 0, "streamed row action receives real layout");
    list->onMouseDown({{frame.x + 8, frame.y + 8}, oneui::MouseButton::Left});
    list->onMouseUp({{frame.x + 8, frame.y + 8}, oneui::MouseButton::Left});
    expect(messages[999]->invokes == 1 && messages[0]->invokes == 0, "template command targets its stable message VM");
    list->setScrollOffset(0);
    expect(instance->rows.at("0") != first && messages[0]->connects == 3, "returning row obtains fresh render lifecycle");
    c.runtime->close();
    for (const auto& item : messages)
        expect(item->retains == item->releases && item->connects == item->disconnects, "closing virtual owner balances every data/render lease");
}
