#include "fixture.h"
using namespace viewtest;
int main(int argc,char **argv) {
    if(argc!=4)return 2;
    try {
        Case c(argv[1]);c.start();
        auto original=c.runtime->root->element.widget;
        auto input=c.runtime->root->children[1]->element.as<oneui::TextField>();
        auto row=c.runtime->root->children[4]->rows.at("a")->element.widget;
        auto notifier=c.vm.notifier;
        const auto scopes=c.runtime->scopes.size();
        input->onFocusChanged(true);input->setTextComposition(L"拼",1);
        for(int i=0;i<50;++i) {
            c.runtime->reload(c.image);
            expect(c.runtime->root->element.widget==original,"reload root identity");
            expect(c.runtime->root->children[1]->element.widget==input,"reload input identity");
            expect(input->hasTextComposition(),"reload preserved composition");
            expect(c.runtime->root->children[4]->rows.at("a")->element.widget==row,"reload row identity");
            expect(c.vm.notifier==notifier && c.vm.connects==1 && c.vm.retains==1,"root notifier/lease stable");
            expect(c.a.connects==1 && c.b.connects==1 && c.runtime->scopes.size()==scopes,"child leases stable");
        }
        c.vm.text(c.title,"after reload");c.runtime->notify(c.runtime->rootScope,c.title);c.runtime->flush();
        expect(input->text()==L"after reload","observers after reload");
        expect(c.runtime->root->children[0]->element.classes.find("after reload") != std::string::npos &&
               c.runtime->root->children[0]->element.classes.find("initial") == std::string::npos,
               "bound classes replace obsolete tokens after repeated reloads");
        auto edited=c.image;
        const auto buttonNode=c.runtime->root->children[2]->node;
        auto button=c.runtime->root->children[2]->element.as<oneui::Button>();
        for(auto &p:edited.records[oneview::PROP])if(p[0]==buttonNode) {
            const auto &contract=edited.components[edited.records[oneview::NODE][buttonNode][1]];
            if(contract.property(p[1]).name=="text")
                edited.strings[edited.records[oneview::VALU][p[2]][2]]="changed button";
        }
        c.runtime->reload(edited);
        expect(button->text()==L"changed button","reload changed static value in retained widget");
        auto &props=edited.records[oneview::PROP];
        props.erase(std::remove_if(props.begin(),props.end(),[&](auto &r){return r[0]==buttonNode;}),props.end());
        auto &events=edited.records[oneview::EVNT];
        events.erase(std::remove_if(events.begin(),events.end(),[&](auto &r){return r[0]==buttonNode;}),events.end());
        c.runtime->reload(edited);
        expect(button->text().empty(),"deleted static property resets default");
        oneui::KeyEvent enter{};enter.key=oneui::Key::Enter;
        auto invoked=c.vm.invokes;button->onKeyDown(enter);
        expect(c.vm.invokes==invoked,"removed event no longer invokes old command");
        c.runtime->reload(c.image);button->onKeyDown(enter);
        expect(c.vm.invokes==invoked+1,"restored event invokes through retained widget");
        auto bad=c.image;bad.schema.models.at("Root").properties.at("title").name="incompatible";
        try {c.runtime->reload(bad);expect(false,"schema reload accepted");}
        catch(const Failure &e) {expect(e.result==ONEUI_VIEW_SCHEMA_MISMATCH,"schema reload diagnostic");}
        auto host=std::dynamic_pointer_cast<oneui::ui::NativeHostView>(c.runtime->hosts.at("terminal"));
        auto content=std::make_shared<oneui::Label>();host->attach(content,{});
        auto renamed=c.image;
        auto &hostRecord=renamed.records[oneview::HOST].front();
        renamed.strings.push_back("other");hostRecord[1]=uint32_t(renamed.strings.size()-1);
        try {c.runtime->reload(renamed);expect(false,"attached host rename accepted");}
        catch(const Failure &e) {expect(e.result==ONEUI_VIEW_BUSY,"host reload BUSY");}
        expect(host->children().front()==content && c.runtime->hosts.at("terminal")==host,"host rollback");
        auto moved=c.image;
        auto &rootNode=moved.records[oneview::NODE][moved.records[oneview::TMPL][moved.root][1]];
        std::swap(moved.records[oneview::EDGE][rootNode[3]+rootNode[4]-1][1],
                  moved.records[oneview::EDGE][rootNode[3]+rootNode[4]-2][1]);
        try {c.runtime->reload(moved);expect(false,"attached host sibling move accepted");}
        catch(const Failure &e) {expect(e.result==ONEUI_VIEW_BUSY,"host sibling move BUSY");}
        auto removed=c.image;
        --removed.records[oneview::NODE][removed.records[oneview::TMPL][removed.root][1]][4];
        try {c.runtime->reload(removed);expect(false,"attached host removal accepted");}
        catch(const Failure &e) {expect(e.result==ONEUI_VIEW_BUSY,"host removal BUSY");}
        auto removeInput=load(argv[2]);removeInput.strings.push_back("renamed");
        removeInput.records[oneview::HOST].front()[1]=uint32_t(removeInput.strings.size()-1);
        try {c.runtime->reload(removeInput);expect(false,"attached host rename with disappearing IME accepted");}
        catch(const Failure &e) {expect(e.result==ONEUI_VIEW_BUSY,"host checked before composition commit");}
        expect(input->hasTextComposition(),"BUSY keeps composition pending");
        host->detach();c.runtime->reload(renamed);
        expect(c.runtime->hosts.count("other"),"detached host rename accepted");
        input->setTextComposition(L"提交",2);
        c.runtime->reload(load(argv[2]));
        expect(c.vm.values.at(c.title).text.find("提交")!=std::string::npos,"removed input commits composition into VM");
        expect(c.runtime->root->children[1]->element.component=="Text","input replaced after composition");
        auto empty=load(argv[3]),withRows=load(argv[2]);
        for(int i=0;i<50;++i) {
            c.runtime->reload(empty);
            expect(c.runtime->scopes.size()==1,"removed list releases cached scopes");
            expect(c.a.retains==c.a.releases && c.b.retains==c.b.releases,"removed list leases balanced");
            expect(c.a.connects==c.a.disconnects,"removed list disconnects subscriptions");
            c.runtime->reload(withRows);
            expect(c.runtime->scopes.size()==3 && c.a.retains-c.a.releases==1,"restored list has one lease per item");
        }
        auto previousRoot=c.runtime->root;
        c.vm.onGet=[&] { c.runtime->notify(c.runtime->rootScope,c.title); };
        try {c.runtime->reload(withRows);expect(false,"get notify reentry accepted");}
        catch(const Failure &e) {expect(e.result==ONEUI_VIEW_REENTRANT,"reload read reentry diagnostic");}
        c.vm.onGet={};
        expect(c.runtime->root==previousRoot,"failed staging leaves old tree intact");
        c.runtime->close();
        expect(c.vm.retains==c.vm.releases && c.a.retains==c.a.releases,"reload final lease balance");
        {
            Case failed(argv[1]);failed.start();auto root=failed.runtime->root;
            auto field=root->children[1]->element.as<oneui::TextField>();
            field->onFocusChanged(true);field->setTextComposition(L"提交",2);
            failed.vm.onGet=[&] {
                if(failed.vm.values.at(failed.title).text.find("提交")!=std::string::npos)
                    failed.runtime->notify(failed.runtime->rootScope,failed.title);
            };
            try {failed.runtime->reload(load(argv[2]));expect(false,"second-pass read failure accepted");}
            catch(const Failure &e) {expect(e.result==ONEUI_VIEW_REENTRANT,"second-pass diagnostic");}
            expect(failed.runtime->root==root && !field->hasTextComposition(),"failed reload preserves tree and committed input");
            failed.vm.onGet={};failed.runtime->flush();
            expect(narrow(field->text())==failed.vm.values.at(failed.title).text,"committed input cache recovers after failed reload");
        }
        std::cout << "PASS reload: 50 retained and 50 remove/restore cycles, leases/subscriptions/IME, schema/Host/reentry rollback\n";
    } catch(const std::exception &e) { std::cerr << e.what() << '\n';return 1; }
}
