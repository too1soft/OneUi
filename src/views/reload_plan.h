#pragma once
#include "runtime.h"
namespace oneui::views {
struct ReloadPlan {
    Runtime &old, &next;
    std::map<const Instance *,std::shared_ptr<Instance>> matches;
    std::map<const Widget *,std::shared_ptr<Widget>> widgets;
    std::set<const Widget *> semanticWidgets,oldSemanticWidgets;
    std::vector<std::shared_ptr<View::ChildrenUpdate>> children;
    std::vector<std::function<void()>> properties;
    std::vector<std::function<void()>> rollback;
    bool committed=false;
    ReloadPlan(Runtime &,Runtime &);
    ~ReloadPlan();
    void match(const std::shared_ptr<Instance> &,const std::shared_ptr<Instance> &);
    void native(const std::shared_ptr<Widget> &,const std::shared_ptr<Widget> &);
    void prepare();
    bool commitComposition();
    std::shared_ptr<Instance> adopt(const std::shared_ptr<Instance> &);
};
std::map<std::string,Value> authored(Runtime &,const Instance &);
std::function<void()> resetProperty(ui::Mount &,ui::Element &,const ui::Element &,const std::string &);
}
