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
    runtime->close();
    std::cout << "code editor declarative binding scenarios passed\n";
}
