#pragma once
#include "oneui/view.h"
#include "oneui/style_sheet.h"
#include <array>
namespace oneui {
class IconButton;
class ONEUI_API PaneHeaderSlot final : public View {
public:
    std::string name;
    Size naturalSize() const override {return children().empty()?Size{}:children()[0]->naturalSize();}
protected:
    void layoutChildren() override {if(!children().empty())children()[0]->setFrame(frame());}
};
enum class PaneHeaderAction { Activate, Split, Maximize, Close, Broadcast, Live, Menu };
struct PaneHeaderConfig {
    std::string pane, broadcastRole="none";
    std::wstring title, path, broadcastHint, broadcastLabel;
    int64_t number=1;
    bool active=false, viewingHistory=false, canClose=true, canSplit=true, canMaximize=true;
};
class ONEUI_API PaneHeader final : public View {
public:
    PaneHeader();
    void setConfig(PaneHeaderConfig);
    const PaneHeaderConfig& config() const {return config_;}
    void setStyleBox(StyleBox);
    void setOnIntent(std::function<void(PaneHeaderAction,Point)>);
    // Same phases as Tabs: start, move, drop, cancel. Window logical points.
    void setOnDrag(std::function<void(int,Point)>);
    void setSlot(const std::string&,std::shared_ptr<Widget>);
    Rect actionFrame(PaneHeaderAction) const;
    bool compact() const {return frame().width<320;}
    Size naturalSize() const override {return {220,29};}
    void paint(Canvas&) override;
    bool onMouseDown(const MouseEvent&) override;
    bool onMouseMove(const MouseEvent&) override;
    bool onMouseUp(const MouseEvent&) override;
    bool onKeyDown(const KeyEvent&) override;
protected:
    void layoutChildren() override;
    bool hasInteractionState() const override;
    void resetInteractionState() override;
private:
    PaneHeaderConfig config_;
    StyleBox style_;
    std::array<std::shared_ptr<IconButton>,6> buttons_;
    std::shared_ptr<Widget> identity_,actions_,menu_;
    std::function<void(PaneHeaderAction,Point)> intent_;
    std::function<void(int,Point)> drag_;
    bool hovered_=false,armed_=false,dragging_=false,grip_=false;
    Point origin_,point_;
    void emit(PaneHeaderAction,Point={});
};
} // namespace oneui
