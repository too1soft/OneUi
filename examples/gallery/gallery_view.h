#pragma once

#include "oneui/controls/badge.h"
#include "oneui/controls/button.h"
#include "oneui/controls/checkbox.h"
#include "oneui/controls/form_field.h"
#include "oneui/controls/label.h"
#include "oneui/controls/list.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/radio_group.h"
#include "oneui/controls/select.h"
#include "oneui/controls/separator.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/table.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/validation_message.h"
#include "oneui/layout/dock_view.h"
#include "oneui/layout/grid.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/split_view.h"
#include "oneui/layout/stack.h"
#include "oneui/layout/wrap.h"
#include "oneui/platform/window.h"
#include "oneui/reactive.h"
#include "oneui/view.h"

#include <memory>
#include <string>

namespace oneui::gallery {

class GalleryView final : public View {
public:
    GalleryView();

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onFocusChanged(bool focused) override;

private:
    void layoutChildren() override;
    void selectSection(int section);
    void updateSectionVisibility();

    int hoveredTarget_ = -1;
    int pressedTarget_ = -1;
    int focusedTarget_ = 0;
    int selectedSection_ = 0;
    bool hasWindowFocus_ = false;

    State<std::wstring> projectName_{L"OneUI app"};
    State<std::wstring> projectKey_{L"one ui"};
    State<bool> projectKeyInvalid_{true};
    State<bool> livePreview_{true};
    State<bool> focusRings_{true};
    State<double> density_{0.68};
    State<int> mode_{0};
    State<int> densityPreset_{1};
    State<int> platformTarget_{0};
    State<int> releaseChannel_{1};
    State<int> selectDispatchCount_{0};
    State<int> activeRecord_{1};
    State<bool> chinese_{false};
    State<std::wstring> previewAction_{L"Hide"};
    State<std::wstring> selectDispatchNote_{L"Open a Select, then click the button or another Select."};
    State<bool> popupOpen_{false};
    State<bool> controlledPopupOpen_{false};
    State<bool> menuOpen_{false};
    bool popupDisabled_ = true;
    int popupPlacement_ = 0;
    int backgroundClickCount_ = 0;
    std::wstring popupEventNote_{L"Open a mode to compare background and outside-click behavior."};

    std::shared_ptr<Stack> statusBadges_;
    std::shared_ptr<Separator> dataSeparator_;
    std::shared_ptr<List> recordList_;
    std::shared_ptr<Table> compactTable_;
    std::shared_ptr<Button> primaryButton_;
    std::shared_ptr<Button> secondaryButton_;
    std::shared_ptr<Button> pressedButton_;
    std::shared_ptr<Stack> buttonRow_;
    std::shared_ptr<Stack> formStack_;
    std::shared_ptr<Stack> styleSettingsStack_;
    std::shared_ptr<Grid> statGrid_;
    std::shared_ptr<DockView> appShell_;
    std::shared_ptr<Wrap> chipWrap_;
    std::shared_ptr<SplitView> inspectorSplit_;
    std::shared_ptr<ScrollView> scrollDemo_;
    std::shared_ptr<Stack> scrollContent_;
    std::shared_ptr<Tabs> modeTabs_;
    std::shared_ptr<TextField> projectNameField_;
    std::shared_ptr<TextField> projectKeyField_;
    std::shared_ptr<TextField> passwordPreviewField_;
    std::shared_ptr<Select> platformSelect_;
    std::shared_ptr<Select> releaseChannelSelect_;
    std::shared_ptr<Switch> livePreviewSwitch_;
    std::shared_ptr<Button> selectDispatchButton_;
    std::shared_ptr<FormField> projectNameFormField_;
    std::shared_ptr<FormField> projectKeyFormField_;
    std::shared_ptr<FormField> platformFormField_;
    std::shared_ptr<FormField> livePreviewFormField_;
    std::shared_ptr<ValidationMessage> formMessage_;
    std::shared_ptr<Popup> basicPopup_;
    std::shared_ptr<Popup> menuPopup_;
    std::shared_ptr<Popup> controlledPopup_;
    std::shared_ptr<OverlayHost> overlayHost_;
    std::shared_ptr<Widget> overlayBaseLayer_;
    std::shared_ptr<Widget> overlayTopLayer_;
    std::shared_ptr<Checkbox> focusRingCheckbox_;
    std::shared_ptr<Checkbox> languageCheckbox_;
    std::shared_ptr<Checkbox> styleFocusRingCheckbox_;
    std::shared_ptr<Slider> densitySlider_;
    std::shared_ptr<RadioGroup> densityPresetGroup_;
    std::shared_ptr<ProgressBar> releaseProgress_;
};

} // namespace oneui::gallery
