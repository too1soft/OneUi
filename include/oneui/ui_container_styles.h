#pragma once
// Included after ui_declarative.h's definitions. Query application belongs to
// declarative composition; the parser/evaluator and isolated layout are core.
namespace oneui::ui {
inline void StyleRegistry::applyBaseLayouts() {
    std::map<const Widget*,std::shared_ptr<Stack>> parents;
    for (const auto& entry : entries_)
        if (auto stack = std::dynamic_pointer_cast<Stack>(entry.widget.lock()))
            for (const auto& child : stack->children()) parents[child.get()] = stack;
    for (const auto& entry : entries_) if (auto widget = entry.widget.lock()) {
        auto parent = parents.find(widget.get());
        if (parent == parents.end()) continue;
        StyleNode node{syntax::components().at(entry.component), {"one-"+entry.component}, 0};
        std::istringstream words(entry.classes);
        for (std::string word; words >> word;) node.classes.push_back(word);
        const auto box = sheet_.resolve(node);
        if (box.width || box.height || box.minWidth || box.minHeight ||
            box.flexBasis || box.flexGrow || box.flexShrink)
            applyLayout(entry, box, parent->second);
    }
}
inline void StyleRegistry::restoreQueryLayouts() {
    for(const auto& entry:entries_)if(auto widget=entry.widget.lock()) {
        auto found=layoutDefaults_.find(widget.get());if(found==layoutDefaults_.end())continue;
        StyleNode node{syntax::components().at(entry.component),{"one-"+entry.component},0};
        std::istringstream words(entry.classes);for(std::string word;words>>word;)node.classes.push_back(word);
        applyLayout(entry,sheet_.resolve(node),found->second.parent.lock());
    }
}
inline void StyleRegistry::configureQuery(const std::shared_ptr<Widget>& widget,const StyleBox& box) {
    auto stack=std::dynamic_pointer_cast<Stack>(widget);
    if(!box.containerName&&!box.sizeContainment) {
        if(stack){stack->setSizeContainment(false);stack->setContainerQuery({});}
        return;
    }
    if(!stack||!box.containerName||box.containerName->empty()||!box.sizeContainment.value_or(false))
        throw std::invalid_argument("CONTAINER_SIZE: a named Stack requires container-type:size");
    stack->setSizeContainment(true);
    queries_[widget.get()].version=~size_t{};
    std::weak_ptr<Stack> weak=stack;std::weak_ptr<int> life=queryLifetime_;
    stack->setContainerQuery([this,weak,life]{if(life.expired())return false;auto w=weak.lock();return w&&evaluateQuery(w);});
}
inline void StyleRegistry::applyLayout(const Entry& entry,const StyleBox& box,const std::shared_ptr<Stack>& parent) {
    auto widget=entry.widget.lock();if(!widget)return;
    auto& defaults=layoutDefaults_[widget.get()];
    if(defaults.widget.lock()!=widget) {
        defaults.widget=widget;defaults.size=widget->preferredSize();defaults.parent=parent;
        if(parent)defaults.flex=parent->explicitFlex(widget);
        if(auto stack=std::dynamic_pointer_cast<Stack>(widget)){defaults.direction=stack->direction();defaults.wrap=stack->wrap();}
    }
    widget->setPreferredSize({box.width.value_or(defaults.size.width),box.height.value_or(defaults.size.height)});
    if(auto page=std::dynamic_pointer_cast<ContainerPage>(widget))page->setDisplay(box.display);
    else widget->setLayoutVisible(box.display.value_or(true));
    if(auto stack=std::dynamic_pointer_cast<Stack>(widget)) {
        stack->setDirection(box.flexDirection?(*box.flexDirection=="row"?StackDirection::Row:StackDirection::Column):defaults.direction);
        if(!stack->setWrap(box.flexWrap.value_or(defaults.wrap)))throw std::invalid_argument("CONTAINER_WRAP: Yoga required");
    } else if(box.flexDirection||box.flexWrap||box.containerName)
        throw std::invalid_argument("CONTAINER_CONSUMPTION: flex direction/wrap/container requires Stack");
    if(parent) {
        auto flex=defaults.flex.value_or(StackFlex{});
        if(box.flexBasis)flex.basis=box.flexBasis;
        if(box.flexGrow)flex.grow=*box.flexGrow;
        if(box.flexShrink)flex.shrink=*box.flexShrink;
        flex.width=box.width;flex.height=box.height;
        flex.minWidth=box.minWidth.value_or(0);flex.minHeight=box.minHeight.value_or(0);
        if(defaults.flex||box.flexBasis||box.flexGrow||box.flexShrink||box.width||box.height||box.minWidth||box.minHeight)parent->setFlex(widget,flex);
        else parent->clearFlex(widget);
    }
}
inline bool StyleRegistry::evaluateQuery(const std::shared_ptr<Stack>& container) {
    try {
        auto& state=queries_[container.get()];auto frame=container->frame();
        if(state.version==sheet_.version()&&state.treeVersion==treeVersion_&&
           state.measured.width==frame.width&&state.measured.height==frame.height)return false;
        auto nodeFor=[](const Entry& entry) {
            StyleNode node{syntax::components().at(entry.component),{"one-"+entry.component},0};
            std::istringstream words(entry.classes);for(std::string word;words>>word;)node.classes.push_back(word);
            return node;
        };
        std::map<const Widget*,const Entry*> entries;
        std::map<const Widget*,std::shared_ptr<View>> parents;
        for(const auto& entry:entries_)if(auto widget=entry.widget.lock()) {
            entries.emplace(widget.get(),&entry);
            if(auto view=std::dynamic_pointer_cast<View>(widget))for(auto& child:view->children())parents[child.get()]=view;
        }
        auto base=sheet_.resolve(nodeFor(*entries.at(container.get())));
        for(const auto& rule:sheet_.rules())if(rule.query&&rule.query->name==base.containerName &&
            selectorMatches(rule.selector,nodeFor(*entries.at(container.get()))))
            throw std::invalid_argument("CONTAINER_DEPENDENCY: a container cannot query its own size");
        auto parent=std::dynamic_pointer_cast<Stack>(parents[container.get()]);
        if(parent) {
            auto flex=parent->explicitFlex(container);bool column=parent->direction()==StackDirection::Column;
            bool main=bool(column?base.height:base.width)||(flex&&(flex->basis.has_value()||flex->grow>0));
            bool cross=parent->align()==StackAlign::Stretch||bool(column?base.width:base.height);
            if(!main||!cross)throw std::invalid_argument("CONTAINER_SIZE: parent must allocate both axes independently");
        }
        std::map<std::string,Size> sizes;
        std::vector<std::shared_ptr<Widget>> ancestors;
        for(std::shared_ptr<Widget> widget=container;widget;) {
            ancestors.push_back(widget);auto found=parents.find(widget.get());widget=found==parents.end()?nullptr:found->second;
        }
        for(auto it=ancestors.rbegin();it!=ancestors.rend();++it)if(entries.count(it->get())) {
            auto box=sheet_.resolve(nodeFor(*entries.at(it->get())));
            if(box.containerName) {
                auto frame=(*it)->frame();auto border=std::max(0.f,box.borderWidth.value_or(0));
                sizes[*box.containerName]={std::max(0.f,frame.width-2*border),std::max(0.f,frame.height-2*border)};
            }
        }
        std::vector<bool> matches;
        for(const auto& rule:sheet_.rules())if(rule.query) {
            auto size=sizes.find(rule.query->name);matches.push_back(size!=sizes.end()&&rule.query->matches(size->second));
        }
        state.measured={frame.width,frame.height};
        if(state.version==sheet_.version()&&state.treeVersion==treeVersion_&&matches==state.matches)return false;
        auto effective=sheet_.forContainers(sizes);
        struct Pending {const Entry* entry;StyleBox box;std::shared_ptr<Stack> parent;};
        std::vector<Pending> pending;
        std::vector<std::shared_ptr<Widget>> work(container->children().begin(),container->children().end());
        while(!work.empty()) {
            auto widget=work.back();work.pop_back();auto found=entries.find(widget.get());bool nested=false;
            if(found!=entries.end()) {
                auto node=nodeFor(*found->second);auto box=effective.resolve(node);auto unconditional=sheet_.resolve(node);
                nested=bool(unconditional.containerName);
                if(nested&&!(unconditional.width&&unconditional.height))
                    throw std::invalid_argument("CONTAINER_DEPENDENCY: nested query containers require fixed independent width and height");
                if(nested)for(const auto& rule:sheet_.rules())if(rule.query&&selectorMatches(rule.selector,node)) {
                    const auto& b=rule.box;
                    if(b.width||b.height||b.minWidth||b.minHeight||b.flexBasis||b.flexGrow||b.flexShrink||b.padding||b.display)
                        throw std::invalid_argument("CONTAINER_DEPENDENCY: ancestor query changes a measured container");
                }
                if((box.flexDirection||box.flexWrap)&&!std::dynamic_pointer_cast<Stack>(widget))
                    throw std::invalid_argument("CONTAINER_CONSUMPTION: layout output requires Stack");
                pending.push_back({found->second,std::move(box),std::dynamic_pointer_cast<Stack>(parents[widget.get()])});
            }
            if(!nested)if(auto view=std::dynamic_pointer_cast<View>(widget))for(auto& child:view->children())work.push_back(child);
        }
        // All conditions use the same immutable measurement snapshot. No
        // condition is evaluated again after this batch changes child layout.
        auto previousFocus=container->activeFocusChild();
        while(previousFocus) {auto next=previousFocus->activeFocusChild();if(!next)break;previousFocus=std::move(next);}
        for(auto& change:pending) {
            applyLayout(*change.entry,change.box,change.parent);
            apply(*change.entry,effective);
        }
        if(previousFocus&&!previousFocus->focused())container->focusFirstLeaf();
        state.matches=std::move(matches);state.version=sheet_.version();state.treeVersion=treeVersion_;
        container->setSizeContainment(true);
        return !pending.empty();
    }catch(const std::exception& error) {
        container->setSizeContainment(false);
        if(queryDiagnostics_.empty()||queryDiagnostics_.back()!=error.what()) {
            if(queryDiagnostics_.size()==32)queryDiagnostics_.erase(queryDiagnostics_.begin());
            queryDiagnostics_.push_back(error.what());
        }
        return false;
    }
}
} // namespace oneui::ui
