#include "fixture.h"
using namespace viewtest;
#include "settings_lists.inc"
#include "slot_contracts.inc"
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    try {
        {
            Case c(argv[1]);
            c.start();
            auto label = c.runtime->root->children[0]->element.as<oneui::Label>();
            expect(label->text() == L"initial", "initial UTF8");
            const auto initialClasses = c.runtime->root->children[0]->element.classes;
            expect(initialClasses.size() > std::string("initial ").size() &&
                   initialClasses.find("initial") != std::string::npos, "scope plus bound classes");
            expect(c.vm.reads[c.title] == 1, "one get per property across bindings");
            c.vm.text(c.title, "changed");
            for (int i = 0; i < 100; ++i)
                c.runtime->notify(c.runtime->rootScope, c.title);
            c.runtime->flush();
            expect(label->text() == L"changed" && c.vm.reads[c.title] == 2, "coalesced change");
            const auto changedClasses = c.runtime->root->children[0]->element.classes;
            expect(changedClasses.substr(0, changedClasses.size() - 7) == initialClasses.substr(0, initialClasses.size() - 7) &&
                   changedClasses.find("changed") != std::string::npos &&
                   changedClasses.find("initial") == std::string::npos,
                   "class updates remove obsolete values and retain scope tokens");
            c.runtime->flush();
            expect(c.vm.reads[c.title] == 2, "no polling");
            expect(c.runtime->hosts.count("terminal") == 1, "empty native host");
            expect(c.runtime->hosts.at("terminal")->naturalSize().width == 0, "empty host size");
            c.vm.text(c.title, "\xe4\xb8\xad\xf0\x9f\x9a\x80");
            c.runtime->notify(c.runtime->rootScope, c.title);
            c.runtime->flush();
            expect(label->text() == L"\u4e2d\U0001f680", "native converter keeps supplementary Unicode");
            c.vm.text(c.title, "\xc0\x80");
            c.runtime->notify(c.runtime->rootScope, c.title);
            try {
                c.runtime->flush();
                expect(false, "invalid UTF8 accepted by converter");
            } catch (const Failure& e) {
                expect(e.result == ONEUI_VIEW_TYPE && label->text() == L"\u4e2d\U0001f680",
                       "UTF8 rejection precedes native conversion");
            }
            c.runtime->close();
            expect(c.vm.retains == 1 && c.vm.releases == 1, "root lease");
            expect(c.a.retains == c.a.releases && c.b.retains == c.b.releases,
                   "item lease balance");
        }
        {
            Case c(argv[1]);
            c.start();
            auto repeat = c.runtime->root->children[4];
            c.runtime->factory.set(c.runtime->root->element,"class",std::string("flow-parent"));
            c.runtime->styles->replace(oneui::ui::syntax::css("Column.flow-parent { gap:17px; }"));
            c.runtime->refreshRepeatLayouts(c.runtime->root);
            expect(repeat->element.as<oneui::Stack>()->gap()==17,
                   "CSS replacement restores inherited repeat spacing");
            auto old = repeat->rows.at("a")->element.widget;
            c.vm.items = {&c.b, &c.a};
            ++c.vm.revision;
            c.runtime->notify(c.runtime->rootScope, c.rows);
            c.runtime->flush();
            expect(repeat->rows.at("a")->element.widget == old, "RESET retains key and VM");
            Vm replacement(c.image, "Item");
            replacement.text(replacement.key, "a");
            replacement.text(id(c.image, "Item", "text"), "new");
            c.vm.items = {&c.b, &replacement};
            ++c.vm.revision;
            c.runtime->notify(c.runtime->rootScope, c.rows);
            c.runtime->flush();
            expect(repeat->rows.at("a")->element.widget != old && c.a.releases == c.a.retains,
                   "RESET replaces changed VM");
            c.runtime->close();
            expect(replacement.retains == replacement.releases, "replacement released");
        }
        {
            Case c(argv[1]);
            c.vm.table.schema_hash[0] ^= 1;
            try {
                c.start();
                expect(false, "schema mismatch accepted");
            } catch (const Failure &e) {
                expect(e.result == ONEUI_VIEW_SCHEMA_MISMATCH, "schema error");
            }
            expect(c.vm.retains == 0, "schema check before callbacks");
        }
        {
            Case c(argv[1]);
            c.vm.connectResult = ONEUI_VIEW_FOREIGN_FAILURE;
            try {
                c.start();
                expect(false, "connect failure accepted");
            } catch (const Failure &) {
            }
            expect(c.vm.connects == 1 && c.vm.disconnects == 1 && c.vm.retains == c.vm.releases,
                   "failed connect rollback");
        }
        {
            Case c(argv[1]);
            c.start();
            c.vm.onGet = [&] {
                try {
                    c.runtime->notify(c.runtime->rootScope, c.title);
                    expect(false, "get notify accepted");
                } catch (const Failure &e) {
                    expect(e.result == ONEUI_VIEW_REENTRANT, "get notify code");
                }
            };
            c.runtime->notify(c.runtime->rootScope, c.title);
            try {
                c.runtime->flush();
                expect(false, "flush hides reentrancy");
            } catch (const Failure &e) {
                expect(e.result == ONEUI_VIEW_REENTRANT, "flush reentrancy code");
            }
            c.vm.onGet = {};
        }
        {
            Case c(argv[1]);
            c.start();
            c.vm.onInvoke = [&] {
                c.runtime->close();
                expect(c.vm.releases == 0, "release inside invoke");
            };
            auto button = c.runtime->root->children[2];
            c.runtime->invoke(button, button->events[0], {});
            expect(c.vm.releases == 1 && c.runtime->closed, "deferred release without pump");
        }
        {
            auto path = std::filesystem::path(argv[1]).parent_path() / "Overlay.oneview";
            auto image = load(path.string().c_str());
            Vm vm(image, "Root");
            vm.text(id(image, "Root", "title"), "overlay");
            vm.values[id(image, "Root", "enabled")].integer = 1;
            auto runtime = std::make_shared<Runtime>(image);
            runtime->initialize(&vm.table, &vm);
            expect(runtime->root->children.size() == 1, "CALL introduces no node");
            auto child = runtime->root->children.front();
            expect(child->element.as<oneui::Button>()->text() == L"overlay",
                   "CALL binding overrides root static");
            expect(child->element.classes.find("inner") != std::string::npos &&
                   child->element.classes.find("outer") != std::string::npos &&
                   child->element.classes.find("inner") < child->element.classes.find("outer"),
                   "CALL class order");
            runtime->invoke(child, child->events.front(), {});
            expect(vm.invokes == 1, "CALL event");
            runtime->close();
        }
        {
            auto path = std::filesystem::path(argv[1]).parent_path() / "Advanced.oneview";
            auto image = load(path.string().c_str());
            Vm vm(image, "Root"), item(image, "Item"), leaf(image, "Leaf");
            vm.text(id(image, "Root", "title"), "caller");
            item.text(item.key, "item");
            item.text(id(image, "Item", "text"), "callee");
            leaf.text(leaf.key, "leaf");
            leaf.text(id(image, "Leaf", "text"), "nested");
            vm.items = {&item};
            item.items = {&leaf};
            auto runtime = std::make_shared<Runtime>(image);
            runtime->initialize(&vm.table, &vm);
            auto row = runtime->root->children[0]->rows.at("item");
            expect(row->children[0]->element.as<oneui::Label>()->text() == L"callee",
                   "callee item scope");
            expect(row->children[1]->children[0]->element.as<oneui::Label>()->text() == L"caller",
                   "slot captures caller scope");
            expect(row->children[2]->rows.at("leaf")->element.as<oneui::Label>()->text() ==
                       L"nested",
                   "nested item scope");
            runtime->close();
            expect(leaf.retains == leaf.releases && item.retains == item.releases,
                   "nested leases balanced");
        }
        {
            Case c(argv[1]);
            auto clipboard = std::make_shared<oneui::MemoryClipboard>();
            c.runtime = std::make_shared<Runtime>(c.image, clipboard);
            c.runtime->initialize(&c.vm.table, &c.vm);
            auto input = c.runtime->root->children[1]->element.as<oneui::TextField>();
            expect(input->clipboard() == clipboard, "runtime uses injected clipboard");
            clipboard->setText(L"platform-free");
            input->pasteFromClipboard(*input->clipboard());
            expect(input->text().find(L"platform-free") != std::wstring::npos,
                   "injected clipboard supports editing without platform library");
            oneui::ui::Mount mount({}, std::make_shared<oneui::ui::StyleRegistry>(), clipboard);
            oneui::State<std::vector<std::shared_ptr<int>>> items({std::make_shared<int>(1)});
            auto rows = mount.repeat(items, [](auto item) { return std::to_wstring(*item); },
                [&](auto &child, auto) { return child.make("Input"); });
            auto field = std::dynamic_pointer_cast<oneui::TextField>(rows.template as<oneui::Stack>()->children()[0]);
            expect(field->clipboard() == clipboard, "legacy repeat propagates clipboard service");
            c.runtime->close();
        }
        settingsLists(argv[1]);
        slotContracts(argv[1]);
        std::cout << "11 runtime scenarios passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
