#pragma once
#include "oneui/layout/pane_grid.h"
namespace oneui {
// One instance per View and authored domain. It owns no content or session.
class ONEUI_API WorkspaceDragDomain {
public:
    void registerGrid(std::shared_ptr<PaneGrid>);
    std::function<void()> prepareGrid(std::shared_ptr<PaneGrid>);
    void registerTabs(std::string id,std::shared_ptr<Widget>);
    bool begin(std::string source,Point,bool tabSource=false);
    void move(Point);
    void release(Point);
    void cancel();
    bool active() const {return !source_.empty();}
private:
    struct Strip {std::string id;std::weak_ptr<Widget> widget;};
    std::vector<std::weak_ptr<PaneGrid>> grids_;
    std::vector<Strip> strips_;
    std::weak_ptr<PaneGrid> origin_,previewOwner_;
    std::string source_;
    int64_t revision_=0;
    bool tabSource_=false;
    size_t registrations_=0;
    bool current();
};
} // namespace oneui
