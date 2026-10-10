#include "capi/oneui_c_api_view_internal.h"
#include "fixture.h"
#include <thread>
using namespace viewtest;
namespace {
OneUiUtf8String utf8(const std::string &s) { return {s.data(), s.size()}; }
OneUiUtf8String utf8(const char *s) { return {s, std::strlen(s)}; }
struct ScopeLease {
    OneUiViewNotifier *value = nullptr;
    ~ScopeLease() { oneui_view_notifier_release(value); }
};
struct Owner {
    int retain = 0, release = 0;
    static void keep(void *p) { ++static_cast<Owner *>(p)->retain; }
    static void drop(void *p) { ++static_cast<Owner *>(p)->release; }
};
int resolve(OneUiView &view, const std::string &key, ScopeLease &out,
            const std::string &instance = "panes") {
    OneUiViewScopeSegment path{utf8(instance), utf8(key)};
    return oneui_view_resolve_scope(&view, &path, 1, &out.value);
}
OneUiWidget *host(OneUiView &view, ScopeLease &scope) {
    OneUiWidget *result = nullptr;
    expect(oneui_view_find_host(&view, scope.value, utf8("terminal"), &result) == 0,
           "scoped host lookup");
    return result;
}
void delta(Case &c, uint32_t kind, uint64_t index, std::string key, uint64_t to = 0) {
    auto k = utf8(key);
    OneUiViewListChange change{};
    change.struct_size = sizeof(change);
    change.kind = kind;
    change.before_revision = c.vm.revision++;
    change.after_revision = c.vm.revision;
    change.index = index;
    change.count = 1;
    change.to_index = to;
    change.keys = &k;
    change.key_count = 1;
    c.runtime->notifyList(c.runtime->rootScope, c.rows, change);
}
void lifecycle(const char *file) {
    Case c(file);
    const std::string key = u8"session/终端";
    c.a.text(c.a.key, key);
    Vm leaf(c.image, "Leaf");
    leaf.text(leaf.key, "inner/slash");
    c.a.lists[id(c.image, "Item", "children")] = {&leaf};
    c.start();
    OneUiView view;
    view.value = c.runtime;
    ScopeLease a, mirror, root;
    expect(resolve(view, key, a) == 0 && resolve(view, key, mirror, "mirrorCall/mirrors") == 0,
           "two repeats");
    expect(a.value != mirror.value, "same VM independent render connections");
    auto *original = host(view, a), *other = host(view, mirror);
    expect(original != other, "same host name separate instances");
    expect(oneui_view_resolve_scope(&view, nullptr, 0, &root.value) == 0, "empty path root");
    ScopeLease nested;
    OneUiViewScopeSegment path[]{{utf8("panes"), utf8(key)}, {utf8("leaves"), utf8("inner/slash")}};
    expect(oneui_view_resolve_scope(&view, path, 2, &nested.value) == 0,
           "nested path Unicode/slash");
    expect(host(view, nested) != original, "nested host identity");
    auto grid =
        std::dynamic_pointer_cast<oneui::View>(c.runtime->root->children[0]->element.widget);
    expect(grid->children().size() == 2, "transparent repeat: native parent owns panes directly");
    expect(grid->children()[0] ==
               c.runtime->root->children[0]->children[0]->rows.at(key)->element.widget,
           "no native Column repeat wrapper");
    Owner owner;
    OneUiWidget content;
    content.widget = std::make_shared<oneui::Panel>();
    expect(oneui_view_host_attach(original, &content, &owner, Owner::keep, Owner::drop) == 0,
           "attach lease");
    expect(oneui_view_host_attach(other, &content, &owner, Owner::keep, Owner::drop) ==
               ONEUI_VIEW_BUSY,
           "cross attach rejected");
    expect(owner.retain == 1 && owner.release == 0, "attach rejection balance");
    // A render connection can emit its own child notification.
    c.a.text(id(c.image, "Item", "text"), "changed");
    expect(oneui_view_notifier_notify(a.value, id(c.image, "Item", "text")) == 0, "row notify");
    c.runtime->flush();
    c.a.text(id(c.image, "Item", "text"), "reset-update");
    ++c.vm.revision;
    c.runtime->notify(c.runtime->rootScope, c.rows);
    c.runtime->flush();
    auto rowInstance = c.runtime->root->children[0]->children[0]->rows.at(key);
    expect(rowInstance->children[0]->element.as<oneui::Label>()->text() == L"reset-update",
           "RESET refreshes retained render properties");
    for (int i = 0; i < 500; ++i) {
        std::swap(c.vm.items[0], c.vm.items[1]);
        delta(c, ONEUI_LIST_MOVE, 0, i % 2 == 0 ? key : "b", 1);
        c.runtime->flush();
        expect(host(view, a) == original, "MOVE preserves NativeHost handle");
        expect(content.widget->hasCompositionOwner() && owner.retain == 1 && owner.release == 0,
               "MOVE preserves content lease");
    }
    auto image = c.runtime->image;
    c.runtime->reload(image);
    expect(host(view, a) == original && owner.release == 0, "reload reuses attached dynamic host");
    // Renaming a mounted host must reject the candidate atomically.
    auto changed = image;
    auto name = changed.sid("renamed");
    for (auto &h : changed.records[oneview::HOST]) {
        h[1] = name;
        for (auto &p : changed.records[oneview::PROP])
            if (p[0] == h[0]) {
                const auto &contract = changed.components[changed.records[oneview::NODE][p[0]][1]];
                if (contract.property(p[1]).name == "host")
                    changed.records[oneview::VALU][p[2]][2] = name;
            }
    }
    bool busy = false;
    try {
        c.runtime->reload(changed);
    } catch (const Failure &e) {
        busy = e.result == ONEUI_VIEW_BUSY;
    }
    expect(busy && owner.release == 0 && host(view, a) == original, "BUSY keeps host and lease");
    // Insertion connection failure cannot remove the old scene or release content.
    Vm bad(c.image, "Item");
    bad.text(bad.key, "bad");
    bad.connectResult = ONEUI_VIEW_FOREIGN_FAILURE;
    c.vm.items.push_back(&bad);
    delta(c, ONEUI_LIST_INSERT, 2, "bad");
    bool failed = false;
    try {
        c.runtime->flush();
    } catch (const Failure &) {
        failed = true;
    }
    expect(failed && host(view, a) == original && owner.release == 0, "failed insert rolls back");
    c.vm.items.pop_back();
    ++c.vm.revision;
    c.runtime->notify(c.runtime->rootScope, c.rows);
    c.runtime->flush();
    expect(bad.retains == bad.releases && bad.connects == bad.disconnects,
           "failed connection balanced");
    c.vm.items.erase(c.vm.items.begin());
    delta(c, ONEUI_LIST_REMOVE, 0, key);
    c.runtime->flush();
    expect(oneui_view_flush(&view) == 0 && !original->widget,
           "retired handle releases native widget");
    expect(owner.release == 1 && !content.widget->hasCompositionOwner(),
           "remove releases after native detach");
    expect(oneui_view_notifier_notify(a.value, id(c.image, "Item", "text")) == ONEUI_VIEW_CLOSED,
           "removed scope permanently closed");
    expect(oneui_view_host_detach(original) == ONEUI_VIEW_CLOSED, "removed host handle rejected");
    ScopeLease missing;
    expect(resolve(view, key, missing) == ONEUI_VIEW_NOT_FOUND && !missing.value,
           "missing output cleared");
    c.vm.items.insert(c.vm.items.begin(), &c.a);
    delta(c, ONEUI_LIST_INSERT, 0, key);
    c.runtime->flush();
    ScopeLease replacement;
    expect(resolve(view, key, replacement) == 0, "reinsert scope");
    expect(replacement.value != a.value && host(view, replacement) != original,
           "same key never revives old handle");
    expect(oneui_view_notifier_notify(nested.value, 0) == ONEUI_VIEW_CLOSED,
           "nested removed closed");
    c.runtime->close();
    expect(c.a.retains == c.a.releases && c.a.connects == c.a.disconnects, "row lifetime balance");
    expect(c.b.retains == c.b.releases && leaf.retains == leaf.releases, "all row leases balanced");
}
void invalid(const char *file) {
    Case c(file), d(file);
    c.start();
    d.start();
    OneUiView view, foreign;
    view.value = c.runtime;
    foreign.value = d.runtime;
    ScopeLease a;
    expect(resolve(view, "a", a) == 0, "resolve");
    OneUiWidget *dataHost = reinterpret_cast<OneUiWidget *>(1);
    auto dataScope = c.runtime->list(c.runtime->rootScope, c.rows).items.front();
    expect(oneui_view_find_host(&view, dataScope->notifier, utf8("terminal"), &dataHost) ==
               ONEUI_VIEW_INVALID_ARGUMENT && !dataHost,
           "data-only notifier cannot alias root host scope");
    OneUiWidget *result = reinterpret_cast<OneUiWidget *>(1);
    expect(oneui_view_find_host(&foreign, a.value, utf8("terminal"), &result) ==
                   ONEUI_VIEW_INVALID_ARGUMENT &&
               !result,
           "foreign view rejected");
    int wrong = 0;
    std::thread worker([&] {
        ScopeLease x;
        wrong = resolve(view, "a", x);
    });
    worker.join();
    expect(wrong == ONEUI_VIEW_WRONG_THREAD, "wrong thread");
    auto sameVm = std::make_shared<Runtime>(c.image);
    sameVm->initialize(&c.vm.table, &c.vm);
    OneUiView sameView;
    sameView.value = sameVm;
    ScopeLease same;
    expect(resolve(sameView, "a", same) == 0 && same.value != a.value, "same VM two Views");
    expect(host(sameView, same) != host(view, a), "same VM two independent content slots");
    sameVm->close();
    ScopeLease bad;
    OneUiViewScopeSegment empty{utf8("panes"), utf8("")};
    expect(oneui_view_resolve_scope(&view, &empty, 1, &bad.value) == ONEUI_VIEW_INVALID_ARGUMENT,
           "empty key");
    expect(oneui_view_resolve_scope(&view, &empty, 65, &bad.value) == ONEUI_VIEW_LIMIT,
           "bounded depth before array read");
    const std::string malformed(1, char(0xff));
    empty.key = utf8(malformed);
    expect(oneui_view_resolve_scope(&view, &empty, 1, &bad.value) == ONEUI_VIEW_INVALID_ARGUMENT,
           "invalid UTF8");
    {
        Callback guard(*c.runtime, Phase::Read);
        expect(resolve(view, "a", bad) == ONEUI_VIEW_REENTRANT, "callback reentry rejected");
    }
    c.runtime->close();
    expect(resolve(view, "a", bad) == ONEUI_VIEW_CLOSED, "closed view");
}
void transferAndAllocationFailure(const char *file) {
    Case c(file);
    c.start();
    OneUiView view;
    view.value = c.runtime;
    ScopeLease a, b;
    expect(resolve(view, "a", a) == 0 && resolve(view, "b", b) == 0, "transfer scopes");
    auto *left = host(view, a), *right = host(view, b);
    Owner owner;
    OneUiWidget content;
    content.widget = std::make_shared<oneui::Panel>();
    auto *identity = content.widget.get();
    expect(oneui_view_host_attach(left, &content, &owner, Owner::keep, Owner::drop) == 0,
           "transfer source");
    expect(oneui_view_host_detach(left) == 0, "detach before transfer");
    expect(oneui_view_host_attach(right, &content, &owner, Owner::keep, Owner::drop) == 0,
           "transfer target");
    expect(content.widget.get() == identity && owner.retain == 2 && owner.release == 1,
           "transfer keeps content object");
    Vm bad(c.image, "Item");
    bad.text(bad.key, "allocation-failure");
    bad.onConnect = [] { throw std::bad_alloc(); };
    c.vm.items.push_back(&bad);
    delta(c, ONEUI_LIST_INSERT, 2, "allocation-failure");
    expect(oneui_view_flush(&view) != ONEUI_VIEW_OK, "allocation failure contained by C boundary");
    expect(host(view, b) == right && owner.release == 1,
           "allocation failure retains published lease");
    expect(bad.retains == bad.releases, "allocation failure balances new VM lease");
    c.vm.items.pop_back();
    ++c.vm.revision;
    c.runtime->notify(c.runtime->rootScope, c.rows);
    expect(oneui_view_flush(&view) == 0, "recover after allocation failure");
    c.runtime->close();
    expect(owner.release == 2, "transfer closes final lease once");
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    try {
        lifecycle(argv[1]);
        invalid(argv[1]);
        transferAndAllocationFailure(argv[1]);
        std::cout << "workspace scope/host: PASS\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
