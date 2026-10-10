#include "reload_plan.h"
#include "oneui/ui_theme.h"
namespace oneui::views {
namespace {
struct Snapshot {
    std::shared_ptr<Scope> scope;
    std::set<uint32_t> dirty;
    std::map<uint32_t,Value> values;
    std::map<uint32_t,ListState> lists;
    std::map<uint32_t,std::vector<std::weak_ptr<Instance>>> observers;
    explicit Snapshot(const std::shared_ptr<Scope> &s):scope(s),dirty(s->dirty),values(s->values),
        lists(s->lists),observers(s->observers){}
    void restore() noexcept {
        scope->dirty.swap(dirty);scope->values.swap(values);scope->lists.swap(lists);scope->observers.swap(observers);
    }
};
struct Candidate {
    Runtime &owner;
    std::shared_ptr<Runtime> value;
    std::vector<Snapshot> snapshots;
    size_t previous;
    bool committed=false;
    Candidate(Runtime &r,oneview::Image image):owner(r),
        value(std::make_shared<Runtime>(std::move(image),r.factory.clipboard())),previous(r.scopes.size()) {
        for(auto &scope:r.scopes)snapshots.emplace_back(scope);
        value->rootScope=r.rootScope;value->scopes=r.scopes;
    }
    ~Candidate() {
        if(!committed) {
            for(size_t i=previous;i<value->scopes.size();++i)value->closeScope(value->scopes[i]);
            for(auto &snapshot:snapshots)snapshot.restore();
        }
        // Borrowed old scopes belong to owner; this staging runtime must never close them.
        value->closed=true;value->rootScope.reset();value->scopes.clear();
    }
    void build() {
        const auto errors=owner.error.sequence;
        auto css=owner.externalCss.empty() ? ui::declarativeTheme() : owner.externalCss;
        for(auto &r:value->image.records[oneview::CSS])css+='\n'+value->image.strings[r[2]];
        value->styles->replace(css,owner.density);
        for(auto &scope:value->scopes) {scope->values.clear();scope->observers.clear();scope->dirty.clear();}
        value->root=value->build(value->image.records[oneview::TMPL][value->image.root][1],{value->rootScope});
        value->update(value->root);
        owner.alive();
        check(owner.error.sequence==errors,owner.error.result,"Callback failed during reload staging");
    }
};
void refresh(Runtime &r,const std::shared_ptr<Instance> &node) {
    node->applying=true;
    for(auto owner:node->owners) {
        auto root=oneview::componentRoot(r.image,owner);
        const auto &contract=r.image.components[r.image.records[oneview::NODE][root][1]];
        for(auto &p:r.image.records[oneview::PROP])
            if(p[0]==owner && contract.property(p[1]).name!="class")
                setScalar(r.factory,node->element,contract.property(p[1]).name,r.constant(p[2]));
    }
    node->applying=false;
    for(auto &child:node->children)refresh(r,child);
    for(auto &[_,row]:node->rows)refresh(r,row);
}
}
void Runtime::reload(oneview::Image replacement) {
    onThread();alive();
    check(!depth && !flushing && callbackPhase==Phase::Idle,ONEUI_VIEW_BUSY,"Reload during callback/transaction");
    check(oneview::sha256(replacement.schema.encode())==hash,ONEUI_VIEW_SCHEMA_MISMATCH,"Reload schema mismatch");
    flush();
    // At most two passes: first discover disappearing composition, then reread
    // the VM after committing it. BUSY Host checks precede any input mutation.
    bool compositionCommitted=false;
    for(unsigned pass=0;pass<2;++pass) {
        if(compositionCommitted) {
            // The first candidate restored its borrowed cache snapshot. Reapply
            // committed input notifications before any second staging attempt,
            // so a later failure cannot leave the retained tree's cache stale.
            for(auto &scope:scopes)if(!scope->closed)notify(scope,0);
            flush();
        }
        Candidate candidate(*this,replacement);
        {
            Callback lease(*this,Phase::Read);
            candidate.build();
        }
        alive();
        ReloadPlan plan(*this,*candidate.value);
        const auto beforeComposition=error.sequence;
        if(pass==0 && plan.commitComposition()) {
            check(error.sequence==beforeComposition,error.result,"Composition writeback failed during reload");
            compositionCommitted=true;
            // Restore staged caches before the next pass. Committed VM input
            // is deliberately retained even if the following reload fails.
            continue;
        }
        plan.prepare();
        candidate.value->styles->remapWidgets(plan.widgets);
        auto nextRoot=plan.adopt(candidate.value->root);
        for(auto &[name,widget]:candidate.value->hosts)
            if(plan.widgets.count(widget.get()))widget=plan.widgets.at(widget.get());
        for(auto &scope:candidate.value->scopes)
            if(scope->notifier)scope->notifier->runtime=weak_from_this();
        image=std::move(candidate.value->image);
        scopeClasses=std::move(candidate.value->scopeClasses);
        root=std::move(nextRoot);hosts=std::move(candidate.value->hosts);
        scopes=std::move(candidate.value->scopes);
        modelWrites=std::move(candidate.value->modelWrites);
        *styles=std::move(*candidate.value->styles);
        candidate.committed=true;
        plan.committed=true;
        for(auto &action:plan.properties)action();
        for(auto &change:plan.children)change->commit();
        for(auto &[_,widget]:plan.widgets) {
            if(auto scroll=std::dynamic_pointer_cast<ScrollView>(widget))scroll->syncPreparedContent();
            if(auto split=std::dynamic_pointer_cast<SplitView>(widget))split->syncPreparedContent();
            if(auto panes=std::dynamic_pointer_cast<AdaptivePanes>(widget))panes->syncPreparedContent();
        }
        styles->applyAll();
        refresh(*this,root);
        update(root);
        // Rebuild dependency observers against adopted nodes, not staging instances.
        for(auto &scope:scopes)scope->observers.clear();
        std::function<void(const std::shared_ptr<Instance>&)> dependencies=[&](const auto &n) {
            n->dirty=true;for(auto &c:n->children)dependencies(c);for(auto &[_,row]:n->rows)dependencies(row);
        };
        dependencies(root);update(root);
        // List caches are leases, not permanent VM ownership. Retain only the
        // lists observed by the new tree, including nested builtin lists.
        std::set<Scope*> reachable;
        std::function<void(const std::shared_ptr<Scope>&)> visit=[&](const auto &scope) {
            if(!reachable.insert(scope.get()).second)return;
            for(auto it=scope->lists.begin();it!=scope->lists.end();) {
                auto observed=scope->observers.find(it->first);
                if(observed==scope->observers.end() || observed->second.empty())
                    it=scope->lists.erase(it);
                else {for(auto &item:it->second.items)visit(item);++it;}
            }
        };
        visit(rootScope);
        for(auto &scope:scopes)if(!reachable.count(scope.get()))closeScope(scope);
        scopes.erase(std::remove_if(scopes.begin(),scopes.end(),
            [](const auto &scope){return scope->closed;}),scopes.end());
        factory.collectUnusedOwned();
        if(republish)republish();
        return;
    }
}
}
