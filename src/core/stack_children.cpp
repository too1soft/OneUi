#include "oneui/layout/stack.h"
#include <algorithm>
#include <stdexcept>
namespace oneui {
void Stack::insert(size_t index,std::shared_ptr<Widget> child) {
    if(!child)throw std::invalid_argument("Stack insert requires a child");
    auto next=children();auto old=std::find(next.begin(),next.end(),child);
    if(old!=next.end())next.erase(old);
    else if(child->hasCompositionOwner())throw std::invalid_argument("Remove child from its current parent first");
    if(index>next.size())throw std::out_of_range("Stack insert index");
    next.insert(next.begin()+index,std::move(child));
    auto plan=prepareChildren(std::move(next));plan->commit();
    invalidate();
}
bool Stack::remove(const std::shared_ptr<Widget>& child) {
    auto next=children();auto found=std::find(next.begin(),next.end(),child);
    if(found==next.end())return false;
    const bool moveFocus=focusedChild()==child.get();
    next.erase(found);auto plan=prepareChildren(std::move(next));plan->commit();
    clearFlex(child);
    for(auto i=flex_.begin();i!=flex_.end();)if(i->first.expired())i=flex_.erase(i);else ++i;
    if(moveFocus)focusFirstLeaf();
    invalidate();return true;
}
} // namespace oneui
