#include "fixture.h"
using namespace viewtest;
namespace {
#ifdef NDEBUG
constexpr int debugProbe = 0;
#else
constexpr int debugProbe = 1;
#endif
OneUiViewListChange delta(uint32_t kind, uint64_t before, uint64_t index, uint64_t count,
                          uint64_t to = 0) {
    OneUiViewListChange d{};
    d.struct_size = sizeof(d);
    d.kind = kind;
    d.before_revision = before;
    d.after_revision = before + 1;
    d.index = index;
    d.count = count;
    d.to_index = to;
    return d;
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    try {
        {
            Case c(argv[1]);
            c.start();
            auto repeat = c.runtime->root->children[4];
            auto a = repeat->rows.at("a")->element.widget;
            OneUiUtf8String key{"a", 1};
            auto d = delta(ONEUI_LIST_MOVE, 1, 0, 1, 1);
            d.keys = &key;
            d.key_count = 1;
            c.vm.items = {&c.b, &c.a};
            ++c.vm.revision;
            c.runtime->notifyList(c.runtime->rootScope, c.rows, d);
            c.runtime->flush();
            expect(c.vm.acquires == 2 + 2 * debugProbe && repeat->rows.at("a")->element.widget == a,
                   "MOVE no reacquire/rebuild");
            d = delta(ONEUI_LIST_REMOVE, 2, 1, 1);
            d.keys = &key;
            d.key_count = 1;
            c.vm.items = {&c.b};
            ++c.vm.revision;
            c.runtime->notifyList(c.runtime->rootScope, c.rows, d);
            c.runtime->flush();
            expect(c.a.releases == 1 + debugProbe && repeat->rows.size() == 1, "REMOVE cached item");
            d = delta(ONEUI_LIST_INSERT, 3, 0, 1);
            d.keys = &key;
            d.key_count = 1;
            c.vm.items = {&c.a, &c.b};
            ++c.vm.revision;
            c.runtime->notifyList(c.runtime->rootScope, c.rows, d);
            c.runtime->flush();
            expect(c.vm.acquires == 3 + 4 * debugProbe && repeat->rows.size() == 2, "INSERT only new acquisition");
            auto text = id(c.image, "Item", "text");
            c.a.text(text, "updated");
            d = delta(ONEUI_LIST_UPDATE, 4, 0, 1);
            d.keys = &key;
            d.key_count = 1;
            d.properties = &text;
            d.property_count = 1;
            ++c.vm.revision;
            c.runtime->notifyList(c.runtime->rootScope, c.rows, d);
            c.runtime->flush();
            expect(repeat->rows.at("a")->element.as<oneui::Label>()->text() == L"updated" &&
                       c.vm.acquires == 3 + 6 * debugProbe,
                   "UPDATE fields only");
        }
        {
            Case c(argv[1]);
            c.start();
            OneUiUtf8String wrong{"missing", 7};
            auto d = delta(ONEUI_LIST_REMOVE, 1, 0, 1);
            d.keys = &wrong;
            d.key_count = 1;
            c.vm.items = {&c.b};
            ++c.vm.revision;
            c.runtime->notifyList(c.runtime->rootScope, c.rows, d);
            try {
                c.runtime->flush();
                expect(false, "stale delta hidden");
            } catch (const Failure &e) {
                expect(e.result == ONEUI_VIEW_STALE_CHANGE, "stale delta diagnosed");
            }
            expect(c.runtime->root->children[4]->rows.size() == 1, "stale delta RESET recovery");
        }
        {
            Case c(argv[1]);
            c.start();
            c.vm.items = {&c.a, &c.a};
            ++c.vm.revision;
            c.runtime->notify(c.runtime->rootScope, c.rows);
            try {
                c.runtime->flush();
                expect(false, "duplicate key accepted");
            } catch (const Failure &e) {
                expect(e.result == ONEUI_VIEW_BAD_KEY, "duplicate key result");
            }
            expect(c.runtime->root->children[4]->rows.size() == 2, "bad snapshot preserves tree");
        }
        {
            Case c(argv[1]);
            c.start();
            auto scope = c.runtime->root->children[4]->rows.at("a")->scopes.front();
            auto text = id(c.image, "Item", "text");
            c.a.text(text, "child-notify");
            c.runtime->notify(scope, text);
            c.runtime->flush();
            expect(c.runtime->root->children[4]->rows.at("a")->element.as<oneui::Label>()->text() ==
                       L"child-notify",
                   "child scope notification");
        }
        {
            Case c(argv[1]);
            c.start();
            auto repeat = c.runtime->root->children[4];
            auto old = repeat->rows.at("a")->element.widget;
            Vm replacement(c.image, "Item");
            replacement.text(replacement.key, "a");
            replacement.text(id(c.image, "Item", "text"), "replacement");
            replacement.onGet = [&] {
                if (replacement.reads[id(c.image, "Item", "text")])
                    throw std::bad_alloc();
            };
            c.vm.items = {&replacement, &c.b};
            ++c.vm.revision;
            c.runtime->notify(c.runtime->rootScope, c.rows);
            try {
                c.runtime->flush();
                expect(false, "allocation fault not propagated");
            } catch (const std::bad_alloc &) {
            }
            expect(repeat->rows.at("a")->element.widget == old && c.a.releases == 0,
                   "failed replacement preserves old tree and VM");
            expect(replacement.retains == replacement.releases,
                   "failed replacement releases new VM");
            replacement.onGet = {};
            c.runtime->flush();
            expect(repeat->rows.at("a")->element.widget != old && c.a.releases == 1,
                   "retry publishes replacement");
            c.runtime->close();
        }
        {
            Case c(argv[1]);
            c.start();
            Vm replacement(c.image, "Item");
            replacement.text(replacement.key, "new");
            replacement.text(id(c.image, "Item", "text"), "new row");
            OneUiUtf8String wrong{"missing", 7};
            auto d = delta(ONEUI_LIST_REMOVE, 1, 0, 1);
            d.keys = &wrong;
            d.key_count = 1;
            c.vm.items = {&replacement};
            ++c.vm.revision;
            c.runtime->notifyList(c.runtime->rootScope, c.rows, d);
            try {
                c.runtime->flush();
                expect(false, "recovery diagnostic absent");
            } catch (const Failure &e) {
                expect(e.result == ONEUI_VIEW_STALE_CHANGE, "recovery error");
            }
            expect(replacement.releases == 0 &&
                       !c.runtime->root->children[4]->rows.at("new")->scopes.front()->closed,
                   "post-commit diagnostic keeps new VM alive");
            c.runtime->close();
            expect(replacement.retains == replacement.releases, "recovered VM released once");
        }
        {
            struct FaultWidget : oneui::Widget {
                void paint(oneui::Canvas &) override {}
                void setTextEnvironment(std::wstring, float) override { throw std::bad_alloc(); }
            };
            oneui::View parent;
            auto old = std::make_shared<oneui::Label>();
            parent.add(old);
            auto next = std::make_shared<FaultWidget>();
            try {
                parent.prepareChildren({next});
                expect(false, "native prepare OOM accepted");
            } catch (const std::bad_alloc &) {
            }
            expect(parent.children().size() == 1 && parent.children().front() == old &&
                       old->hasCompositionOwner() && !next->hasCompositionOwner(),
                   "native prepare strong guarantee");
        }
        {
            Case c(argv[1]);
            c.start();
            Vm replacement(c.image, "Item");
            replacement.text(replacement.key, "a");
            c.vm.items = {&replacement, &c.b};
            auto &state = c.runtime->rootScope->lists.at(c.rows);
            try {
                c.runtime->auditListIdentity(c.runtime->rootScope, c.rows, state, state.keys);
                expect(false, "precise delta silently replaced VM");
            } catch (const Failure &e) {
                expect(e.result == ONEUI_VIEW_STALE_CHANGE, "debug identity diagnostic");
            }
            expect(replacement.retains == replacement.releases && c.a.releases == 0,
                   "debug probe balances lease and preserves original VM");
            c.runtime->close();
        }
        std::cout << "8 list scenarios passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
