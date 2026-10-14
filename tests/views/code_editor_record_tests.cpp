#include "fixture.h"
#include "oneui/controls/code_editor.h"
#include <memory>
using namespace viewtest;
void codeEditorRecordTests(const std::filesystem::path& path) {
    auto image=load(path.string().c_str());
    Vm vm(image,"Script");
    const auto language=id(image,"Script","language"), text=id(image,"Script","text"), revision=id(image,"Script","revision");
    const auto& commands=image.schema.model("Script").commands;
    vm.text(language,R"({"keywords":["SELECT","FROM"],"line_comments":["--"]})");
    vm.text(text,"select 1");
    vm.values[revision].integer=0;
    auto runtime=std::make_shared<Runtime>(image);
    runtime->initialize(&vm.table,&vm);
    auto editor=runtime->root->children[0]->element.as<oneui::CodeEditor>();
    expect(editor!=nullptr,"CodeEditor is created from the template");
    expect(editor->text()==L"select 1","revision 0 text is initial content");
    expect(editor->wordWrap(),"static word-wrap applies");
    expect(!editor->lineTokens(0).empty() && editor->lineTokens(0)[0].kind==oneui::CodeTokenKind::Keyword,
           "bound language colors keywords");
    // Edits raise changed(revision) then cursor(...), both typed.
    const auto invokes=vm.invokes;
    editor->setSelection(8,8);
    editor->insertText(L";");
    // setSelection used the cursor window; deliver the coalesced update now.
    editor->flushNotifications();
    runtime->flush();
    expect(vm.invokes>=invokes+2,"changed and cursor events reach the VM");
    expect(vm.lastCommand==commands.at("cursor").id && vm.lastArgs.size()==5 && vm.lastArgs[0].integer==9 &&
           vm.lastArgs[2].integer==1 && vm.lastArgs[3].integer==10,"cursor(caret, anchor, line, column, cursors)");
    // Initial content no longer overrides the user's edits.
    vm.text(text,"select 2");
    runtime->notify(runtime->rootScope,text);runtime->flush();
    expect(editor->text()==L"select 1;","initial text does not clobber edits");
    // A new revision replaces the document.
    vm.text(text,"select 3");
    vm.values[revision].integer=1;
    runtime->notify(runtime->rootScope,text);runtime->notify(runtime->rootScope,revision);runtime->flush();
    expect(editor->text()==L"select 3","a newer revision replaces the text");
    editor->insertText(L" -- edited");
    runtime->notify(runtime->rootScope,revision);runtime->flush();
    expect(editor->text()==L" -- editedselect 3","the same revision is not applied twice");
    // Decorations bind as records with their own revision.
    const auto marks=id(image,"Script","marks"), marksRevision=id(image,"Script","marksRevision");
    auto mark=std::make_unique<Vm>(image,"Mark");
    mark->text(mark->key,"m1");mark->values[id(image,"Mark","start")].integer=0;mark->values[id(image,"Mark","end")].integer=3;
    mark->text(id(image,"Mark","kind"),"error");mark->text(id(image,"Mark","message"),"unknown word");
    vm.lists[marks]={mark.get()};
    vm.values[marksRevision].integer=static_cast<std::int64_t>(editor->revision());
    ++vm.revision;runtime->notify(runtime->rootScope,marks);runtime->notify(runtime->rootScope,marksRevision);runtime->flush();
    expect(editor->decorations().size()==1 && editor->decorations()[0].kind==oneui::CodeDecorationKind::Error,
           "decorations bind with kinds");
    // Completion: request, answer, accept.
    const auto items=id(image,"Script","items"), open=id(image,"Script","open");
    editor->setText(L"");
    editor->onTextInput(L's');
    runtime->flush();
    expect(vm.lastCommand==commands.at("complete").id && vm.lastArgs.size()==2 && vm.lastArgs[0].integer==1 &&
           vm.lastArgs[1].integer==1,"completionRequested(offset, trigger)");
    std::vector<std::unique_ptr<Vm>> owned;
    for(const char* label:{"select","sessions"}) {
        auto s=std::make_unique<Vm>(image,"Suggestion");
        s->text(s->key,label);s->text(id(image,"Suggestion","label"),label);s->text(id(image,"Suggestion","kind"),"keyword");
        vm.lists[items].push_back(s.get());owned.push_back(std::move(s));
    }
    vm.values[open].integer=1;
    ++vm.revision;runtime->notify(runtime->rootScope,items);runtime->notify(runtime->rootScope,open);runtime->flush();
    expect(editor->completionOpen() && editor->visibleCompletions().size()==2,"completion items and open bind");
    oneui::KeyEvent enter;enter.key=oneui::Key::Enter;
    editor->onKeyDown(enter);runtime->flush();
    expect(editor->text()==L"select" && vm.lastCommand==commands.at("accepted").id && vm.lastArgs.size()==1 &&
           vm.lastArgs[0].text=="select","completionAccepted(id) after closing");
    runtime->close();
    std::cout << "code editor declarative binding scenarios passed\n";
}
