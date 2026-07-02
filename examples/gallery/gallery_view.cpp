#include "gallery_view.h"

#include "oneui/color.h"

#include <array>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>

namespace oneui::gallery {
namespace {

constexpr Color Ink{25, 28, 33};
constexpr Color Muted{97, 103, 114};
constexpr Color Quiet{140, 146, 156};
constexpr Color AppBg{242, 244, 247};
constexpr Color Sidebar{17, 24, 39};
constexpr Color SidebarMuted{148, 163, 184};
constexpr Color Accent{37, 99, 235};
constexpr Color AccentHover{29, 78, 216};
constexpr Color AccentPressed{30, 64, 175};
constexpr Color AccentSoft{219, 234, 254};
constexpr Color Green{22, 163, 74};
constexpr Color GreenSoft{220, 252, 231};
constexpr Color Violet{124, 58, 237};
constexpr Color VioletSoft{237, 233, 254};
constexpr Color Amber{217, 119, 6};
constexpr Color AmberSoft{254, 243, 199};
constexpr Color Line{226, 229, 234};
constexpr float ContentX = 244.0f;
constexpr float ContentY = 144.0f;
constexpr float ContentW = 750.0f;
constexpr float ContentH = 430.0f;
constexpr float InnerX = 264.0f;
constexpr float InnerY = 164.0f;

struct Target {
    int id;
    Rect rect;
};

enum TargetId {
    Section0 = 0,
    Section1 = 1,
    Section2 = 2,
    Section3 = 3,
    Section4 = 4,
    Section5 = 5,
    CreateAction = 6,
    PreviewAction = 7,
    ExportAction = 8,
    TargetCount = 9
};

enum OverlayDemoTargetId {
    OverlayBasicAction = 100,
    OverlayPlacementAction = 101,
    OverlayMenuAction = 102,
    OverlayControlledAction = 103,
    OverlayDisabledAction = 104,
    OverlayBackgroundAction = 105
};

std::array<Target, TargetCount> targets() {
    return {{
        {Section0, Rect{16.0f, 96.0f, 180.0f, 34.0f}},
        {Section1, Rect{16.0f, 138.0f, 180.0f, 34.0f}},
        {Section2, Rect{16.0f, 180.0f, 180.0f, 34.0f}},
        {Section3, Rect{16.0f, 222.0f, 180.0f, 34.0f}},
        {Section4, Rect{16.0f, 264.0f, 180.0f, 34.0f}},
        {Section5, Rect{16.0f, 306.0f, 180.0f, 34.0f}},
        {CreateAction, Rect{592.0f, 26.0f, 82.0f, 34.0f}},
        {PreviewAction, Rect{684.0f, 26.0f, 82.0f, 34.0f}},
        {ExportAction, Rect{776.0f, 26.0f, 82.0f, 34.0f}},
    }};
}

const Target* findTarget(int id) {
    const auto all = targets();
    for (const auto& target : all) {
        if (target.id == id) {
            return &target;
        }
    }
    return nullptr;
}

int hitTarget(Point point) {
    const auto all = targets();
    for (const auto& target : all) {
        if (target.rect.contains(point)) {
            return target.id;
        }
    }
    return -1;
}

int hitOverlayDemoTarget(Point point) {
    const std::array<Target, 6> all{{
        {OverlayBasicAction, Rect{284.0f, 226.0f, 168.0f, 30.0f}},
        {OverlayMenuAction, Rect{524.0f, 226.0f, 168.0f, 30.0f}},
        {OverlayControlledAction, Rect{764.0f, 226.0f, 168.0f, 30.0f}},
        {OverlayBackgroundAction, Rect{284.0f, 414.0f, 190.0f, 30.0f}},
        {OverlayPlacementAction, Rect{524.0f, 414.0f, 168.0f, 30.0f}},
        {OverlayDisabledAction, Rect{764.0f, 414.0f, 168.0f, 30.0f}},
    }};
    for (const auto& target : all) {
        if (target.rect.contains(point)) {
            return target.id;
        }
    }
    return -1;
}

class ModePopupContent final : public Widget {
public:
    ModePopupContent(PopupInteractionMode mode, State<bool>& chinese, State<bool>& openState)
        : mode_(mode), chinese_(chinese), openState_(openState) {
        setPreferredSize(Size{218.0f, 96.0f});
    }

    void paint(Canvas& canvas) override {
        const Rect rect = frame();
        const bool chinese = chinese_.get();
        canvas.drawText(title(chinese), Rect{rect.x + 12.0f, rect.y + 10.0f, rect.width - 24.0f, 18.0f}, Ink, 13.0f, TextAlign::Left);
        canvas.drawText(body(chinese), Rect{rect.x + 12.0f, rect.y + 32.0f, rect.width - 24.0f, 30.0f}, Muted, 12.0f, TextAlign::Left);

        const Rect button = closeRect();
        canvas.fillRect(button, Color{248, 250, 252}, 6.0f);
        canvas.strokeRect(button, Color{203, 213, 225}, 6.0f, 1.0f);
        canvas.drawText(chinese ? L"关闭" : L"Close", button, Ink, 12.0f);
    }

    bool onMouseDown(const MouseEvent& event) override {
        if (!closeRect().contains(event.position)) {
            return true;
        }
        openState_.set(false);
        return true;
    }

    bool onKeyDown(const KeyEvent& event) override {
        if (event.key != Key::Enter && event.key != Key::Space && event.key != Key::Escape) {
            return false;
        }
        openState_.set(false);
        return true;
    }

    bool isFocusable() const override {
        return true;
    }

private:
    const wchar_t* title(bool chinese) const {
        switch (mode_) {
        case PopupInteractionMode::Modeless:
            return chinese ? L"Modeless：背景可操作" : L"Modeless: background stays active";
        case PopupInteractionMode::LightDismiss:
            return chinese ? L"LightDismiss：点外部关闭" : L"LightDismiss: outside click closes";
        case PopupInteractionMode::Modal:
            return chinese ? L"Modal：阻止背景点击" : L"Modal: blocks background clicks";
        }
        return L"";
    }

    const wchar_t* body(bool chinese) const {
        switch (mode_) {
        case PopupInteractionMode::Modeless:
            return chinese ? L"外部点击不会关闭浮层，背景按钮仍会响应。" : L"Outside clicks do not dismiss it; background actions still work.";
        case PopupInteractionMode::LightDismiss:
            return chinese ? L"外部点击或 Escape 会关闭，不锁住背景。" : L"Outside click or Escape dismisses it without locking focus.";
        case PopupInteractionMode::Modal:
            return chinese ? L"背景点击会被消费，请先关闭当前浮层。" : L"Background clicks are consumed until this popup is closed.";
        }
        return L"";
    }

    Rect closeRect() const {
        const Rect rect = frame();
        return Rect{rect.x + 12.0f, rect.y + rect.height - 32.0f, 82.0f, 24.0f};
    }

    PopupInteractionMode mode_ = PopupInteractionMode::Modeless;
    State<bool>& chinese_;
    State<bool>& openState_;
};

const wchar_t* targetLabel(int id, bool chinese) {
    switch (id) {
    case Section0:
        return chinese ? L"总览" : L"Overview";
    case Section1:
        return chinese ? L"控件" : L"Controls";
    case Section2:
        return chinese ? L"数据" : L"Data";
    case Section3:
        return chinese ? L"布局" : L"Layouts";
    case Section4:
        return chinese ? L"样式" : L"Style";
    case Section5:
        return chinese ? L"\u6d6e\u5c42" : L"Overlay";
    case CreateAction:
        return chinese ? L"创建" : L"Create";
    case PreviewAction:
        return chinese ? L"EN" : L"中文";
    case ExportAction:
        return chinese ? L"导出" : L"Export";
    default:
        return L"";
    }
}

const wchar_t* sectionTitle(int section, bool chinese) {
    switch (section) {
    case Section1:
        return chinese ? L"控件" : L"Controls";
    case Section2:
        return chinese ? L"数据展示" : L"Data Display";
    case Section3:
        return chinese ? L"布局" : L"Layouts";
    case Section4:
        return chinese ? L"样式系统" : L"Style System";
    case Section5:
        return chinese ? L"\u6d6e\u5c42\u4e0e Popup" : L"Overlay And Popup";
    case Section0:
    default:
        return chinese ? L"总览" : L"Overview";
    }
}

const wchar_t* sectionSubtitle(int section, bool chinese) {
    switch (section) {
    case Section1:
        return chinese ? L"属性、状态、事件和选择控件。" : L"Props, state, events, and selection controls.";
    case Section2:
        return chinese ? L"状态标记、分隔线、列表和紧凑表格。" : L"Status badges, separators, lists, and compact tables.";
    case Section3:
        return chinese ? L"应用外壳、换行布局和分割面板。" : L"App shells, wrapping rows, and split panes.";
    case Section4:
        return chinese ? L"默认令牌加组件级样式覆盖。" : L"Default tokens plus component-level overrides.";
    case Section5:
        return chinese ? L"\u951a\u70b9\u5b9a\u4f4d\u3001\u83dc\u5355\u6d6e\u5c42\u3001\u53d7\u63a7\u6253\u5f00\u4e0e\u4e8b\u4ef6\u8fb9\u754c\u3002" : L"Anchor placement, menu layers, controlled open state, and event boundaries.";
    case Section0:
    default:
        return chinese ? L"项目状态和当前 OneUI 能力面。" : L"Project health and current OneUI surface.";
    }
}

void label(Canvas& canvas, const std::wstring& text, Rect rect, Color color = Ink, float size = 14.0f) {
    canvas.drawText(text, rect, color, size, TextAlign::Left);
}

void value(Canvas& canvas, const std::wstring& text, Rect rect, Color color = Ink, float size = 24.0f) {
    canvas.drawText(text, rect, color, size, TextAlign::Left);
}

void chip(Canvas& canvas, const std::wstring& text, Rect rect, Color bg, Color fg) {
    canvas.fillRect(rect, bg, 10.0f);
    canvas.drawText(text, rect, fg, 12.0f);
}

void card(Canvas& canvas, Rect rect) {
    canvas.fillRect(Rect{rect.x, rect.y + 2.0f, rect.width, rect.height}, Color{219, 223, 229, 120}, 8.0f);
    canvas.fillRect(rect, colors::Panel, 8.0f);
    canvas.strokeRect(rect, Line, 8.0f, 1.0f);
}

void focusRing(Canvas& canvas, Rect rect, bool visible) {
    if (!visible) {
        return;
    }
    canvas.strokeRect(Rect{rect.x - 3.0f, rect.y - 3.0f, rect.width + 6.0f, rect.height + 6.0f}, Color{96, 165, 250}, 8.0f, 2.0f);
}

void stat(Canvas& canvas, Rect rect, const std::wstring& name, const std::wstring& number, const std::wstring& delta, Color accent, Color soft) {
    card(canvas, rect);
    canvas.fillRect(Rect{rect.x + 16.0f, rect.y + 16.0f, 36.0f, 36.0f}, soft, 8.0f);
    canvas.fillRect(Rect{rect.x + 27.0f, rect.y + 27.0f, 14.0f, 14.0f}, accent, 4.0f);
    label(canvas, name, Rect{rect.x + 64.0f, rect.y + 15.0f, rect.width - 80.0f, 20.0f}, Muted, 13.0f);
    value(canvas, number, Rect{rect.x + 64.0f, rect.y + 38.0f, 96.0f, 28.0f});
    chip(canvas, delta, Rect{rect.x + rect.width - 78.0f, rect.y + 40.0f, 56.0f, 22.0f}, soft, accent);
}

void actionButton(Canvas& canvas, const Target& action, const wchar_t* text, bool hovered, bool pressed, bool focused, bool primary) {
    Color background = primary ? Accent : colors::Panel;
    Color foreground = primary ? colors::White : Ink;
    Color border = primary ? Accent : Color{211, 216, 224};

    if (primary && hovered) {
        background = AccentHover;
        border = background;
    } else if (!primary && hovered) {
        background = Color{248, 250, 252};
    }

    if (pressed) {
        background = primary ? AccentPressed : Color{232, 236, 242};
    }

    focusRing(canvas, action.rect, focused);
    canvas.fillRect(action.rect, background, 6.0f);
    canvas.strokeRect(action.rect, border, 6.0f, 1.0f);
    canvas.drawText(text, action.rect, foreground, 13.0f);
}

class StatCard final : public Widget {
public:
    StatCard(std::wstring name, std::wstring number, std::wstring delta, Color accent, Color soft)
        : name_(std::move(name)), number_(std::move(number)), delta_(std::move(delta)), accent_(accent), soft_(soft) {
        setPreferredSize(Size{0.0f, 88.0f});
    }

    void paint(Canvas& canvas) override {
        const Rect rect = frame();
        card(canvas, rect);
        canvas.fillRect(Rect{rect.x + 16.0f, rect.y + 16.0f, 36.0f, 36.0f}, soft_, 8.0f);
        canvas.fillRect(Rect{rect.x + 27.0f, rect.y + 27.0f, 14.0f, 14.0f}, accent_, 4.0f);
        label(canvas, name_, Rect{rect.x + 64.0f, rect.y + 15.0f, rect.width - 80.0f, 20.0f}, Muted, 13.0f);
        value(canvas, number_, Rect{rect.x + 64.0f, rect.y + 38.0f, 96.0f, 28.0f});
        chip(canvas, delta_, Rect{rect.x + rect.width - 78.0f, rect.y + 40.0f, 56.0f, 22.0f}, soft_, accent_);
    }

private:
    std::wstring name_;
    std::wstring number_;
    std::wstring delta_;
    Color accent_;
    Color soft_;
};

class DemoBlock final : public Widget {
public:
    DemoBlock(std::wstring text, Color background, Color foreground, float radius = 6.0f)
        : text_(std::move(text)), background_(background), foreground_(foreground), radius_(radius) {}

    void paint(Canvas& canvas) override {
        canvas.fillRect(frame(), background_, radius_);
        canvas.drawText(text_, frame(), foreground_, 12.0f);
    }

private:
    std::wstring text_;
    Color background_;
    Color foreground_;
    float radius_ = 6.0f;
};

class ScrollItem final : public Widget {
public:
    explicit ScrollItem(int index) : index_(index) {
        setPreferredSize(Size{620.0f, 34.0f});
    }

    void setChinese(bool chinese) {
        if (chinese_ == chinese) {
            return;
        }
        chinese_ = chinese;
        invalidate();
    }

    void paint(Canvas& canvas) override {
        const Rect rect = frame();
        const auto& t = theme();
        canvas.fillRect(rect, index_ % 2 == 0 ? t.surfaceMuted : t.surface, t.radiusSm);
        const std::wstring title = chinese_ ? L"第 " + std::to_wstring(index_) + L" 项" : L"Item " + std::to_wstring(index_);
        const std::wstring detail = chinese_ ? L"宽内容行：拖动底部滑块查看右侧状态" : L"Wide row: drag the bottom thumb to inspect status";
        label(canvas, title, Rect{rect.x + 10.0f, rect.y + 4.0f, rect.width - 20.0f, 16.0f}, t.text, 12.0f);
        label(canvas, detail, Rect{rect.x + 10.0f, rect.y + 18.0f, rect.width - 20.0f, 14.0f}, t.textMuted, 11.0f);
        const Rect status{rect.x + rect.width - 168.0f, rect.y + 7.0f, 142.0f, 20.0f};
        canvas.fillRect(status, index_ % 3 == 0 ? GreenSoft : AccentSoft, 5.0f);
        label(canvas, index_ % 3 == 0 ? (chinese_ ? L"已同步" : L"Synced") : (chinese_ ? L"待检查" : L"Pending"), status, index_ % 3 == 0 ? Green : Accent, 11.0f);
    }

private:
    int index_ = 0;
    bool chinese_ = false;
};

void buttonSpecimen(Canvas& canvas, Rect rect, const std::wstring& text, Color background, Color foreground, Color border, bool focusVisible) {
    focusRing(canvas, rect, focusVisible);
    canvas.fillRect(rect, background, 6.0f);
    canvas.strokeRect(rect, border, 6.0f, 1.0f);
    canvas.drawText(text, rect, foreground, 13.0f);
}

void popupTrigger(Canvas& canvas, Rect rect, const std::wstring& text, bool active, bool disabled = false) {
    const Color background = disabled ? Color{238, 241, 245} : (active ? Accent : colors::Panel);
    const Color foreground = disabled ? Quiet : (active ? colors::White : Ink);
    const Color border = disabled ? Color{226, 232, 240} : (active ? Accent : Color{211, 216, 224});
    canvas.fillRect(rect, background, 6.0f);
    canvas.strokeRect(rect, border, 6.0f, 1.0f);
    canvas.drawText(text, rect, foreground, 12.0f);
}

void popupSurface(Canvas& canvas, Rect rect) {
    canvas.fillRect(Rect{rect.x, rect.y + 3.0f, rect.width, rect.height}, Color{15, 23, 42, 24}, 8.0f);
    canvas.fillRect(rect, colors::Panel, 8.0f);
    canvas.strokeRect(rect, Color{203, 213, 225}, 8.0f, 1.0f);
}

PopupPreferredPlacement popupPlacementByIndex(int index) {
    switch (index % 6) {
    case 1:
        return PopupPreferredPlacement::TopStart;
    case 2:
        return PopupPreferredPlacement::BottomEnd;
    case 3:
        return PopupPreferredPlacement::TopEnd;
    case 4:
        return PopupPreferredPlacement::LeftStart;
    case 5:
        return PopupPreferredPlacement::RightStart;
    default:
        return PopupPreferredPlacement::BottomStart;
    }
}

std::wstring popupPlacementLabel(PopupPreferredPlacement placement) {
    switch (placement) {
    case PopupPreferredPlacement::BottomStart:
        return L"BottomStart";
    case PopupPreferredPlacement::BottomEnd:
        return L"BottomEnd";
    case PopupPreferredPlacement::TopStart:
        return L"TopStart";
    case PopupPreferredPlacement::TopEnd:
        return L"TopEnd";
    case PopupPreferredPlacement::LeftStart:
        return L"LeftStart";
    case PopupPreferredPlacement::RightStart:
        return L"RightStart";
    }
    return L"BottomStart";
}

void placementDiagram(Canvas& canvas, Rect bounds, PopupPreferredPlacement placement) {
    const Rect viewport{bounds.x + 12.0f, bounds.y + 10.0f, bounds.width - 24.0f, bounds.height - 20.0f};
    const Rect anchor{bounds.x + bounds.width / 2.0f - 18.0f, bounds.y + bounds.height / 2.0f - 10.0f, 36.0f, 20.0f};
    const auto result = PopupPlacement::resolve(PopupPlacementRequest{
        anchor,
        Size{70.0f, 34.0f},
        viewport,
        placement,
        8.0f
    });

    canvas.fillRect(viewport, Color{248, 250, 252}, 8.0f);
    canvas.strokeRect(viewport, Line, 8.0f, 1.0f);
    canvas.fillRect(anchor, Accent, 5.0f);
    canvas.drawText(L"anchor", anchor, colors::White, 10.0f);
    canvas.fillRect(result.rect, result.flipped ? AmberSoft : GreenSoft, 6.0f);
    canvas.strokeRect(result.rect, result.flipped ? Amber : Green, 6.0f, 1.0f);
    canvas.drawText(result.flipped ? L"flip" : L"popup", result.rect, result.flipped ? Amber : Green, 10.0f);
}

bool isProjectKeyValid(const std::wstring& text) {
    if (text.empty()) {
        return false;
    }

    for (wchar_t ch : text) {
        if ((ch >= L'a' && ch <= L'z') || ch == L'-') {
            continue;
        }
        return false;
    }
    return true;
}

std::shared_ptr<Stack> popupContent(std::initializer_list<std::wstring> lines, Size size) {
    auto stack = std::make_shared<Stack>(StackDirection::Column);
    stack->setGap(2.0f);
    stack->setPreferredSize(size);

    bool first = true;
    for (const auto& line : lines) {
        auto text = std::make_shared<Label>(line);
        text->setPreferredSize(Size{0.0f, first ? 22.0f : 18.0f});
        text->setFontSize(first ? 13.0f : 12.0f);
        text->setColor(first ? Ink : Muted);
        stack->add(text);
        first = false;
    }
    return stack;
}

void checkboxSpecimen(Canvas& canvas, Rect rect, bool checked, bool disabled, bool focusVisible) {
    const Rect box{rect.x, rect.y + 5.0f, 16.0f, 16.0f};
    const Color background = disabled ? Color{248, 250, 252} : (checked ? Accent : colors::Panel);
    const Color border = disabled ? Line : (checked ? Accent : Color{211, 216, 224});
    const Color text = disabled ? Quiet : Muted;

    focusRing(canvas, box, focusVisible);
    canvas.fillRect(box, background, 4.0f);
    canvas.strokeRect(box, border, 4.0f, 1.0f);
    if (checked) {
        const Color check = disabled ? Quiet : colors::White;
        canvas.drawLine(Point{box.x + 4.0f, box.y + 8.0f}, Point{box.x + 7.0f, box.y + 11.0f}, check, 2.0f);
        canvas.drawLine(Point{box.x + 7.0f, box.y + 11.0f}, Point{box.x + 12.0f, box.y + 5.0f}, check, 2.0f);
    }
    label(canvas, checked ? L"checked" : L"unchecked", Rect{rect.x + 26.0f, rect.y, rect.width - 26.0f, 26.0f}, text, 12.0f);
}

FormFieldStyleOverride compactFormFieldStyle() {
    FormFieldStyleOverride style;
    style.labelFontSize = 12.0f;
    style.messageFontSize = 11.0f;
    style.labelLineHeight = 16.0f;
    style.messageLineHeight = 14.0f;
    style.labelGap = 2.0f;
    style.controlGap = 2.0f;
    return style;
}

} // namespace

GalleryView::GalleryView() {
    primaryButton_ = std::make_shared<Button>(L"Save");
    primaryButton_->setOnClick([this] {
        selectSection(Section1);
    });

    secondaryButton_ = std::make_shared<Button>(L"Cancel");
    secondaryButton_->setVariant(ButtonVariant::Secondary);
    secondaryButton_->bindDisabled(livePreview_);
    secondaryButton_->setOnClick([this] {
        projectName_.set(L"OneUI app");
    });

    pressedButton_ = std::make_shared<Button>(L"Hide");
    pressedButton_->bindText(previewAction_);
    pressedButton_->setOnClick([this] {
        const bool next = !livePreview_.get();
        livePreview_.set(next);
        previewAction_.set(next ? L"Hide" : L"Show");
    });

    modeTabs_ = std::make_shared<Tabs>();
    modeTabs_->setItems({L"Props", L"State", L"Events"});
    modeTabs_->bindSelectedIndex(mode_);
    TabsStyleOverride modeTabsStyle;
    TabsStateStyleOverride modeTabsNormal;
    modeTabsNormal.background = Color{239, 244, 251};
    modeTabsNormal.border = Line;
    modeTabsNormal.itemForeground = Muted;
    modeTabsStyle.normal = modeTabsNormal;
    TabsStateStyleOverride modeTabsHovered;
    modeTabsHovered.itemBackground = AccentSoft;
    modeTabsStyle.hovered = modeTabsHovered;
    TabsStateStyleOverride modeTabsSelected;
    modeTabsSelected.selectedItemBackground = colors::White;
    modeTabsSelected.selectedItemForeground = Accent;
    modeTabsSelected.selectedItemBorder = Color{203, 213, 225};
    modeTabsStyle.selected = modeTabsSelected;
    modeTabs_->setStyleOverride(modeTabsStyle);

    buttonRow_ = std::make_shared<Stack>(StackDirection::Row);
    buttonRow_->setGap(12.0f);
    buttonRow_->setAlign(StackAlign::Start);
    buttonRow_->add(primaryButton_);
    buttonRow_->add(secondaryButton_);
    buttonRow_->add(pressedButton_);

    statGrid_ = std::make_shared<Grid>(3);
    statGrid_->setColumnGap(20.0f);
    statGrid_->setAutoRows(88.0f);
    statGrid_->add(std::make_shared<StatCard>(L"Components", L"24", L"+6", Accent, AccentSoft));
    statGrid_->add(std::make_shared<StatCard>(L"Tokens", L"42", L"Live", Violet, VioletSoft));
    statGrid_->add(std::make_shared<StatCard>(L"Backends", L"3", L"Plan", Amber, AmberSoft));

    projectNameField_ = std::make_shared<TextField>(L"Project name");
    projectNameField_->bindText(projectName_);
    projectNameField_->setClipboard(std::make_shared<SystemClipboard>());
    projectNameField_->setPreferredSize(Size{270.0f, 26.0f});

    projectKeyField_ = std::make_shared<TextField>(L"one-ui");
    projectKeyField_->bindText(projectKey_);
    projectKeyField_->setClipboard(std::make_shared<SystemClipboard>());
    projectKeyField_->setPreferredSize(Size{270.0f, 26.0f});
    projectKeyField_->setOnChanged([this](const std::wstring& text) {
        projectKeyInvalid_.set(!isProjectKeyValid(text));
    });

    passwordPreviewField_ = std::make_shared<TextField>(L"Password");
    passwordPreviewField_->setText(L"secret-token-2026-long");
    passwordPreviewField_->setPasswordMode(true);
    passwordPreviewField_->setClipboard(std::make_shared<SystemClipboard>());
    passwordPreviewField_->setPreferredSize(Size{150.0f, 30.0f});

    platformSelect_ = std::make_shared<Select>();
    platformSelect_->setItems({L"Windows", L"Linux", L"macOS"});
    platformSelect_->bindSelectedIndex(platformTarget_);
    platformSelect_->setPreferredSize(Size{270.0f, 26.0f});
    SelectStyleOverride platformSelectStyle;
    SelectStateStyleOverride platformSelectNormal;
    platformSelectNormal.padding = Insets{0.0f, 12.0f};
    platformSelectStyle.normal = platformSelectNormal;
    SelectStateStyleOverride platformSelectDisabled;
    platformSelectDisabled.background = Color{245, 247, 250};
    platformSelectDisabled.foreground = Quiet;
    platformSelectDisabled.border = Line;
    platformSelectDisabled.arrowColor = Quiet;
    platformSelectStyle.disabled = platformSelectDisabled;
    platformSelect_->setStyleOverride(platformSelectStyle);
    platformSelect_->setOnChanged([this](int index) {
        const std::array<const wchar_t*, 3> names{L"Windows", L"Linux", L"macOS"};
        selectDispatchNote_.set(chinese_.get()
            ? (std::wstring(L"目标平台已切换到 ") + names[static_cast<std::size_t>(index)] + L"。")
            : (std::wstring(L"Platform changed to ") + names[static_cast<std::size_t>(index)] + L"."));
    });

    releaseChannelSelect_ = std::make_shared<Select>();
    releaseChannelSelect_->setItems({L"Stable", L"Beta", L"Canary"});
    releaseChannelSelect_->bindSelectedIndex(releaseChannel_);
    releaseChannelSelect_->setPreferredSize(Size{150.0f, 30.0f});
    releaseChannelSelect_->setOnChanged([this](int index) {
        const std::array<const wchar_t*, 3> names{L"Stable", L"Beta", L"Canary"};
        selectDispatchNote_.set(chinese_.get()
            ? (std::wstring(L"发布通道已切换到 ") + names[static_cast<std::size_t>(index)] + L"。")
            : (std::wstring(L"Release channel changed to ") + names[static_cast<std::size_t>(index)] + L"."));
    });

    selectDispatchButton_ = std::make_shared<Button>(L"Check / 派发");
    selectDispatchButton_->setVariant(ButtonVariant::Secondary);
    selectDispatchButton_->setPreferredSize(Size{150.0f, 30.0f});
    selectDispatchButton_->setOnClick([this] {
        const int next = selectDispatchCount_.get() + 1;
        selectDispatchCount_.set(next);
        selectDispatchNote_.set(chinese_.get()
            ? (std::wstring(L"外部按钮已收到点击 ") + std::to_wstring(next) + L" 次；打开的 Select 会先关闭。")
            : (std::wstring(L"Outside button received click ") + std::to_wstring(next) + L"; any open Select closes first."));
    });

    livePreviewSwitch_ = std::make_shared<Switch>(L"Enable live preview");
    livePreviewSwitch_->bindChecked(livePreview_);
    livePreviewSwitch_->setPreferredSize(Size{190.0f, 24.0f});

    const FormFieldStyleOverride formFieldStyle = compactFormFieldStyle();
    projectNameFormField_ = std::make_shared<FormField>();
    projectNameFormField_->setLabel(L"项目名称 / Project name");
    projectNameFormField_->setHelperText(L"必填，显示在标题中 / Required, shown in the title.");
    projectNameFormField_->setRequired(true);
    projectNameFormField_->setStyleOverride(formFieldStyle);
    projectNameFormField_->setChild(projectNameField_);

    projectKeyFormField_ = std::make_shared<FormField>();
    projectKeyFormField_->setLabel(L"项目标识 / Project key");
    projectKeyFormField_->setErrorText(L"只能使用小写字母和连字符 / Use lowercase letters and hyphens.");
    projectKeyFormField_->bindInvalid(projectKeyInvalid_);
    projectKeyFormField_->setStyleOverride(formFieldStyle);
    projectKeyFormField_->setChild(projectKeyField_);

    platformFormField_ = std::make_shared<FormField>();
    platformFormField_->setLabel(L"目标平台 / Platform");
    platformFormField_->setHelperText(L"Light-dismiss Select: outside clicks keep dispatching.");
    platformFormField_->setStyleOverride(formFieldStyle);
    platformFormField_->setChild(platformSelect_);

    livePreviewFormField_ = std::make_shared<FormField>();
    livePreviewFormField_->setLabel(L"启用实时预览 / Enable live preview");
    livePreviewFormField_->setHelperText(L"保存前自动刷新预览 / Refresh preview before saving.");
    livePreviewFormField_->setStyleOverride(formFieldStyle);
    livePreviewFormField_->setChild(livePreviewSwitch_);

    formMessage_ = std::make_shared<ValidationMessage>(L"提示信息 / ValidationMessage uses form tokens.");
    formMessage_->setPreferredSize(Size{270.0f, 14.0f});

    focusRingCheckbox_ = std::make_shared<Checkbox>(L"Show keyboard focus rings");
    focusRingCheckbox_->bindChecked(focusRings_);
    focusRingCheckbox_->setPreferredSize(Size{220.0f, 22.0f});

    languageCheckbox_ = std::make_shared<Checkbox>(L"中文界面 / Chinese UI");
    languageCheckbox_->bindChecked(chinese_);
    languageCheckbox_->setPreferredSize(Size{220.0f, 22.0f});

    styleFocusRingCheckbox_ = std::make_shared<Checkbox>(L"Show focus rings / 显示焦点环");
    styleFocusRingCheckbox_->bindChecked(focusRings_);
    styleFocusRingCheckbox_->setPreferredSize(Size{220.0f, 22.0f});

    densitySlider_ = std::make_shared<Slider>();
    densitySlider_->setStep(0.02);
    densitySlider_->setPreferredSize(Size{220.0f, 20.0f});
    densitySlider_->bindValue(density_);

    densityPresetGroup_ = std::make_shared<RadioGroup>();
    densityPresetGroup_->setItems({L"紧凑 Compact", L"均衡 Balanced", L"舒适 Comfortable"});
    densityPresetGroup_->setPreferredSize(Size{210.0f, 84.0f});
    densityPresetGroup_->bindSelectedIndex(densityPreset_);
    RadioGroupStyleOverride densityRadioStyle;
    RadioGroupStateStyleOverride densityRadioNormal;
    densityRadioNormal.indicatorInset = 10.0f;
    densityRadioNormal.labelGap = 8.0f;
    densityRadioNormal.labelColor = Quiet;
    densityRadioStyle.normal = densityRadioNormal;
    RadioGroupStateStyleOverride densityRadioHovered;
    densityRadioHovered.itemBackground = AccentSoft;
    densityRadioStyle.hovered = densityRadioHovered;
    RadioGroupStateStyleOverride densityRadioSelected;
    densityRadioSelected.indicatorBorder = Violet;
    densityRadioSelected.indicatorFill = Violet;
    densityRadioSelected.selectedLabelColor = Ink;
    densityRadioStyle.selected = densityRadioSelected;
    densityPresetGroup_->setStyleOverride(densityRadioStyle);
    densityPresetGroup_->setOnChanged([this](int index) {
        if (index == 0) {
            density_.set(0.42);
        } else if (index == 1) {
            density_.set(0.68);
        } else {
            density_.set(0.88);
        }
    });

    releaseProgress_ = std::make_shared<ProgressBar>();
    releaseProgress_->bindValue(density_);

    appShell_ = std::make_shared<DockView>();
    appShell_->setGap(4.0f);
    appShell_->setPadding(Insets{4.0f});
    auto shellTop = std::make_shared<DemoBlock>(L"header", Color{229, 231, 235}, Ink);
    shellTop->setPreferredSize(Size{0.0f, 18.0f});
    auto shellLeft = std::make_shared<DemoBlock>(L"nav", Color{31, 41, 55}, colors::White);
    shellLeft->setPreferredSize(Size{54.0f, 0.0f});
    auto shellCenter = std::make_shared<DemoBlock>(L"content", Color{239, 246, 255}, Accent);
    appShell_->setTop(shellTop);
    appShell_->setLeft(shellLeft);
    appShell_->setCenter(shellCenter);

    chipWrap_ = std::make_shared<Wrap>();
    chipWrap_->setGap(6.0f);
    chipWrap_->setRowGap(6.0f);
    const std::array<const wchar_t*, 6> chips{L"Build", L"Preview", L"Export", L"Tokens", L"MVVM", L"Skia"};
    const std::array<float, 6> chipWidths{48.0f, 60.0f, 54.0f, 54.0f, 48.0f, 42.0f};
    for (std::size_t i = 0; i < chips.size(); ++i) {
        auto chipView = std::make_shared<DemoBlock>(chips[i], AccentSoft, Accent, 10.0f);
        chipView->setPreferredSize(Size{chipWidths[i], 22.0f});
        chipWrap_->add(chipView);
    }

    inspectorSplit_ = std::make_shared<SplitView>(SplitOrientation::Horizontal);
    inspectorSplit_->setGap(6.0f);
    inspectorSplit_->setSplitRatio(0.68f);
    inspectorSplit_->setFirst(std::make_shared<DemoBlock>(L"document", Color{236, 253, 245}, Green));
    inspectorSplit_->setSecond(std::make_shared<DemoBlock>(L"inspector", VioletSoft, Violet));
    scrollContent_ = std::make_shared<Stack>(StackDirection::Column);
    scrollContent_->setGap(6.0f);
    scrollContent_->setPadding(Insets{8.0f});
    scrollContent_->setPreferredSize(Size{620.0f, 648.0f});
    for (int i = 1; i <= 16; ++i) {
        scrollContent_->add(std::make_shared<ScrollItem>(i));
    }
    scrollDemo_ = std::make_shared<ScrollView>();
    scrollDemo_->setContent(scrollContent_);
    scrollDemo_->setContentWidth(620.0f);
    scrollDemo_->setContentHeight(648.0f);
    scrollDemo_->setHorizontalScrollOffset(72.0f);

    basicPopup_ = std::make_shared<Popup>();
    auto basicAnchor = std::make_shared<Button>(L"Modeless");
    basicAnchor->setPreferredSize(Size{168.0f, 30.0f});
    basicAnchor->setOnClick([this] {
        popupOpen_.set(!popupOpen_.get());
        popupEventNote_ = popupOpen_.get()
            ? (chinese_.get() ? L"Modeless 已打开：现在点击背景按钮，它仍会响应。" : L"Modeless is open: click the background action; it still responds.")
            : (chinese_.get() ? L"Modeless 已关闭。" : L"Modeless closed.");
    });
    basicPopup_->setAnchor(basicAnchor);
    basicPopup_->setContent(std::make_shared<ModePopupContent>(PopupInteractionMode::Modeless, chinese_, popupOpen_));
    basicPopup_->bindOpen(popupOpen_);
    basicPopup_->setInteractionMode(PopupInteractionMode::Modeless);

    menuPopup_ = std::make_shared<Popup>();
    auto menuAnchor = std::make_shared<Button>(L"LightDismiss");
    menuAnchor->setVariant(ButtonVariant::Secondary);
    menuAnchor->setPreferredSize(Size{168.0f, 30.0f});
    menuAnchor->setOnClick([this] {
        menuOpen_.set(!menuOpen_.get());
        popupEventNote_ = menuOpen_.get()
            ? (chinese_.get() ? L"LightDismiss 已打开：点击空白区域会关闭它。" : L"LightDismiss is open: click outside to dismiss it.")
            : (chinese_.get() ? L"LightDismiss 已关闭。" : L"LightDismiss closed.");
    });
    menuPopup_->setAnchor(menuAnchor);
    menuPopup_->setContent(std::make_shared<ModePopupContent>(PopupInteractionMode::LightDismiss, chinese_, menuOpen_));
    menuPopup_->bindOpen(menuOpen_);
    menuPopup_->setInteractionMode(PopupInteractionMode::LightDismiss);

    controlledPopup_ = std::make_shared<Popup>();
    auto controlledAnchor = std::make_shared<Button>(L"Modal");
    controlledAnchor->setPreferredSize(Size{168.0f, 30.0f});
    controlledAnchor->setOnClick([this] {
        controlledPopupOpen_.set(!controlledPopupOpen_.get());
        popupEventNote_ = controlledPopupOpen_.get()
            ? (chinese_.get() ? L"Modal 已打开：背景点击会被阻止，先关闭浮层。" : L"Modal is open: background clicks are blocked until it closes.")
            : (chinese_.get() ? L"Modal 已关闭。" : L"Modal closed.");
    });
    controlledPopup_->setAnchor(controlledAnchor);
    controlledPopup_->setContent(std::make_shared<ModePopupContent>(PopupInteractionMode::Modal, chinese_, controlledPopupOpen_));
    controlledPopup_->bindOpen(controlledPopupOpen_);
    controlledPopup_->setInteractionMode(PopupInteractionMode::Modal);

    overlayHost_ = std::make_shared<OverlayHost>();
    overlayBaseLayer_ = std::make_shared<DemoBlock>(L"layer 0", Color{236, 253, 245}, Green);
    overlayTopLayer_ = std::make_shared<DemoBlock>(L"layer 10", VioletSoft, Violet);
    overlayHost_->addOverlay(overlayBaseLayer_, 0);
    overlayHost_->addOverlay(overlayTopLayer_, 10);

    appShell_->setVisible(false);
    chipWrap_->setVisible(false);
    inspectorSplit_->setVisible(false);
    scrollDemo_->setVisible(false);
    basicPopup_->setVisible(false);
    menuPopup_->setVisible(false);
    controlledPopup_->setVisible(false);
    overlayHost_->setVisible(false);

    statusBadges_ = std::make_shared<Stack>(StackDirection::Row);
    statusBadges_->setGap(8.0f);
    statusBadges_->setAlign(StackAlign::Start);
    const std::array<std::shared_ptr<Badge>, 5> badges{
        std::make_shared<Badge>(L"Info", BadgeVariant::Neutral),
        std::make_shared<Badge>(L"Ok", BadgeVariant::Success),
        std::make_shared<Badge>(L"Warn", BadgeVariant::Warning),
        std::make_shared<Badge>(L"Risk", BadgeVariant::Danger),
        std::make_shared<Badge>(L"New", BadgeVariant::Accent),
    };
    const std::array<float, 5> badgeWidths{48.0f, 48.0f, 54.0f, 50.0f, 48.0f};
    for (std::size_t i = 0; i < badges.size(); ++i) {
        BadgeStyleOverride badgeStyle;
        badgeStyle.borderWidth = 1.0f;
        badgeStyle.radius = 12.0f;
        badgeStyle.padding = Insets{0.0f, 8.0f};
        badgeStyle.fontSize = 12.0f;
        badges[i]->setStyleOverride(badgeStyle);
        badges[i]->setPreferredSize(Size{badgeWidths[i], 24.0f});
        statusBadges_->add(badges[i]);
    }

    dataSeparator_ = std::make_shared<Separator>();
    SeparatorStyleOverride separatorStyle;
    separatorStyle.color = Line;
    separatorStyle.thickness = 1.0f;
    dataSeparator_->setStyleOverride(separatorStyle);

    recordList_ = std::make_shared<List>();
    recordList_->setItems({
        ListItem{L"Acme Console", L"Production workspace"},
        ListItem{L"Billing Portal", L"Pending review"},
        ListItem{L"Ops Dashboard", L"Healthy"},
    });
    recordList_->bindSelectedIndex(activeRecord_);
    ListStyleOverride recordListStyle;
    ListStateStyleOverride recordListNormal;
    recordListNormal.background = colors::White;
    recordListNormal.border = Line;
    recordListNormal.separator = Line;
    recordListNormal.titleColor = Ink;
    recordListNormal.detailColor = Muted;
    recordListNormal.textInset = 12.0f;
    recordListStyle.normal = recordListNormal;
    ListStateStyleOverride recordListHovered;
    recordListHovered.rowBackground = Color{248, 250, 252};
    recordListStyle.hovered = recordListHovered;
    ListStateStyleOverride recordListSelected;
    recordListSelected.selectedRowBackground = AccentSoft;
    recordListSelected.selectedTitleColor = Accent;
    recordListSelected.selectedDetailColor = Muted;
    recordListStyle.selected = recordListSelected;
    recordList_->setStyleOverride(recordListStyle);

    compactTable_ = std::make_shared<Table>();
    compactTable_->setColumns({
        TableColumn{L"Name", 70.0f},
        TableColumn{L"Status", 54.0f},
        TableColumn{L"Owner", 0.0f},
    });
    compactTable_->setRows({
        {L"Acme", L"Live", L"Rina"},
        {L"Billing", L"Review", L"Max"},
        {L"Ops", L"Ready", L"Chen"},
    });
    TableStyleOverride compactTableStyle;
    compactTableStyle.background = colors::White;
    compactTableStyle.border = Line;
    compactTableStyle.headerBackground = Color{248, 250, 252};
    compactTableStyle.headerForeground = Muted;
    compactTableStyle.cellForeground = Ink;
    compactTableStyle.gridLine = Line;
    compactTableStyle.headerHeight = 30.0f;
    compactTableStyle.cellPadding = Insets{0.0f, 8.0f};
    compactTable_->setStyleOverride(compactTableStyle);

    formStack_ = std::make_shared<Stack>(StackDirection::Column);
    formStack_->setGap(2.0f);
    formStack_->setAlign(StackAlign::Start);
    formStack_->add(projectNameFormField_);
    formStack_->add(projectKeyFormField_);
    formStack_->add(platformFormField_);
    formStack_->add(livePreviewFormField_);
    formStack_->add(formMessage_);

    styleSettingsStack_ = std::make_shared<Stack>(StackDirection::Column);
    styleSettingsStack_->setGap(4.0f);
    styleSettingsStack_->setAlign(StackAlign::Start);
    styleSettingsStack_->add(languageCheckbox_);
    styleSettingsStack_->add(styleFocusRingCheckbox_);

    ProgressBarStyleOverride releaseProgressStyle;
    releaseProgressStyle.trackBackground = Color{226, 232, 240};
    releaseProgressStyle.fill = Accent;
    releaseProgressStyle.disabledFill = Muted;
    releaseProgressStyle.radius = 4.0f;
    releaseProgress_->setStyleOverride(releaseProgressStyle);

    add(modeTabs_);
    add(buttonRow_);
    add(statGrid_);
    add(appShell_);
    add(chipWrap_);
    add(inspectorSplit_);
    add(scrollDemo_);
    add(statusBadges_);
    add(dataSeparator_);
    add(recordList_);
    add(compactTable_);
    add(formStack_);
    add(passwordPreviewField_);
    add(releaseChannelSelect_);
    add(selectDispatchButton_);
    add(styleSettingsStack_);
    add(densityPresetGroup_);
    add(releaseProgress_);
    add(overlayHost_);
    add(basicPopup_);
    add(menuPopup_);
    add(controlledPopup_);
    updateSectionVisibility();
}

void GalleryView::layoutChildren() {
    updateSectionVisibility();
    statGrid_->setFrame(Rect{244.0f, 132.0f, 750.0f, 88.0f});
    modeTabs_->setFrame(Rect{264.0f, 204.0f, 260.0f, 30.0f});
    buttonRow_->setFrame(Rect{264.0f, 242.0f, 312.0f, 36.0f});
    formStack_->setFrame(Rect{264.0f, 296.0f, 300.0f, 260.0f});
    styleSettingsStack_->setFrame(Rect{264.0f, 504.0f, 320.0f, 56.0f});
    densityPresetGroup_->setFrame(Rect{764.0f, 254.0f, 210.0f, 84.0f});
    appShell_->setFrame(Rect{672.0f, 206.0f, 302.0f, 82.0f});
    chipWrap_->setFrame(Rect{672.0f, 310.0f, 302.0f, 50.0f});
    inspectorSplit_->setFrame(Rect{672.0f, 386.0f, 302.0f, 54.0f});
    scrollDemo_->setFrame(Rect{264.0f, 220.0f, 330.0f, 240.0f});
    releaseProgress_->setFrame(Rect{264.0f, 360.0f, 520.0f, 10.0f});
    statusBadges_->setFrame(Rect{672.0f, 204.0f, 302.0f, 24.0f});
    dataSeparator_->setFrame(Rect{672.0f, 240.0f, 302.0f, 1.0f});
    recordList_->setFrame(Rect{672.0f, 264.0f, 126.0f, 132.0f});
    compactTable_->setFrame(Rect{812.0f, 264.0f, 162.0f, 132.0f});
    passwordPreviewField_->setFrame(Rect{668.0f, 306.0f, 150.0f, 30.0f});
    releaseChannelSelect_->setFrame(Rect{668.0f, 356.0f, 150.0f, 30.0f});
    selectDispatchButton_->setFrame(Rect{832.0f, 356.0f, 142.0f, 30.0f});
    basicPopup_->setFrame(Rect{284.0f, 226.0f, 168.0f, 30.0f});
    menuPopup_->setFrame(Rect{524.0f, 226.0f, 168.0f, 30.0f});
    controlledPopup_->setFrame(Rect{764.0f, 226.0f, 168.0f, 30.0f});
    const Rect overlayViewport{ContentX, ContentY, ContentW, ContentH};
    basicPopup_->setViewport(overlayViewport);
    menuPopup_->setViewport(overlayViewport);
    controlledPopup_->setViewport(overlayViewport);
    overlayHost_->setFrame(Rect{638.0f, 390.0f, 334.0f, 82.0f});
    overlayBaseLayer_->setFrame(Rect{658.0f, 422.0f, 210.0f, 38.0f});
    overlayTopLayer_->setFrame(Rect{742.0f, 400.0f, 188.0f, 38.0f});
}

void GalleryView::selectSection(int section) {
    if (section < Section0 || section > Section5 || selectedSection_ == section) {
        return;
    }

    selectedSection_ = section;
    updateSectionVisibility();
    invalidate();
}

void GalleryView::updateSectionVisibility() {
    const bool overview = selectedSection_ == Section0;
    const bool controls = selectedSection_ == Section1;
    const bool tokens = selectedSection_ == Section2;
    const bool layouts = selectedSection_ == Section3;
    const bool style = selectedSection_ == Section4;
    const bool overlay = selectedSection_ == Section5;

    statGrid_->setVisible(overview);
    modeTabs_->setVisible(controls);
    buttonRow_->setVisible(controls);
    formStack_->setVisible(controls);
    passwordPreviewField_->setVisible(controls);
    releaseChannelSelect_->setVisible(controls);
    selectDispatchButton_->setVisible(controls);
    appShell_->setVisible(layouts);
    chipWrap_->setVisible(layouts);
    inspectorSplit_->setVisible(layouts);
    scrollDemo_->setVisible(layouts);
    statusBadges_->setVisible(tokens);
    dataSeparator_->setVisible(tokens);
    recordList_->setVisible(tokens);
    compactTable_->setVisible(tokens);
    styleSettingsStack_->setVisible(style);
    densityPresetGroup_->setVisible(style);
    releaseProgress_->setVisible(false);
    basicPopup_->setVisible(overlay);
    menuPopup_->setVisible(overlay);
    controlledPopup_->setVisible(overlay);
    overlayHost_->setVisible(overlay);
}

void GalleryView::paint(Canvas& canvas) {
    const Rect root = frame();
    const bool chinese = chinese_.get();
    if (scrollContent_) {
        for (const auto& child : scrollContent_->children()) {
            if (auto* item = dynamic_cast<ScrollItem*>(child.get())) {
                item->setChinese(chinese);
            }
        }
    }
    canvas.clear(AppBg);

    canvas.fillRect(Rect{0.0f, 0.0f, 212.0f, root.height}, Sidebar, 0.0f);
    canvas.fillRect(Rect{24.0f, 24.0f, 36.0f, 36.0f}, Accent, 8.0f);
    canvas.drawText(L"O", Rect{24.0f, 24.0f, 36.0f, 36.0f}, colors::White, 18.0f);
    label(canvas, L"OneUI", Rect{72.0f, 22.0f, 110.0f, 24.0f}, colors::White, 18.0f);
    label(canvas, chinese ? L"桌面 UI 系统" : L"Desktop UI system", Rect{72.0f, 46.0f, 126.0f, 18.0f}, SidebarMuted, 12.0f);

    const auto allTargets = targets();
    for (int i = 0; i < 6; ++i) {
        const auto& item = allTargets[i];
        if (i == selectedSection_) {
            canvas.fillRect(item.rect, Color{37, 99, 235, 55}, 6.0f);
            canvas.fillRect(Rect{item.rect.x, item.rect.y + 9.0f, 3.0f, 16.0f}, Accent, 2.0f);
        } else if (hoveredTarget_ == item.id) {
            canvas.fillRect(item.rect, Color{255, 255, 255, 18}, 6.0f);
        }
        focusRing(canvas, item.rect, focusRings_.get() && hasWindowFocus_ && focusedTarget_ == item.id);
        label(canvas, targetLabel(item.id, chinese), Rect{item.rect.x + 18.0f, item.rect.y, item.rect.width - 24.0f, item.rect.height}, i == selectedSection_ ? colors::White : SidebarMuted, 13.0f);
    }

    if (root.height >= 460.0f) {
        card(canvas, Rect{20.0f, root.height - 116.0f, 172.0f, 80.0f});
    label(canvas, chinese ? L"渲染器" : L"Renderer", Rect{36.0f, root.height - 100.0f, 96.0f, 18.0f}, Muted, 12.0f);
    label(canvas, L"Skia Raster", Rect{36.0f, root.height - 76.0f, 118.0f, 22.0f}, Ink, 15.0f);
        chip(canvas, L"Win7 path", Rect{36.0f, root.height - 48.0f, 82.0f, 22.0f}, GreenSoft, Green);
    }

    canvas.fillRect(Rect{212.0f, 0.0f, root.width - 212.0f, 86.0f}, colors::Panel, 0.0f);
    canvas.strokeRect(Rect{212.0f, 85.0f, root.width - 212.0f, 1.0f}, Line, 0.0f, 1.0f);
    label(canvas, sectionTitle(selectedSection_, chinese), Rect{244.0f, 20.0f, 240.0f, 28.0f}, Ink, 22.0f);
    label(canvas, sectionSubtitle(selectedSection_, chinese), Rect{244.0f, 50.0f, 520.0f, 20.0f}, Muted, 13.0f);

    for (int i = CreateAction; i <= ExportAction; ++i) {
        const auto& item = allTargets[i];
        actionButton(canvas, item, targetLabel(item.id, chinese), hoveredTarget_ == item.id, pressedTarget_ == item.id, focusRings_.get() && hasWindowFocus_ && focusedTarget_ == item.id, i == CreateAction);
    }

    if (selectedSection_ == Section0) {
        card(canvas, Rect{244.0f, 254.0f, 750.0f, 250.0f});
        label(canvas, chinese ? L"广度优先构建" : L"Breadth-first build", Rect{264.0f, 276.0f, 220.0f, 24.0f}, Ink, 17.0f);
        label(canvas, chinese ? L"核心控件、布局骨架和数据展示能力已经拆分到独立页面。" : L"Core controls, layout skeletons, and data display primitives are now separated into Gallery sections.", Rect{264.0f, 304.0f, 650.0f, 20.0f}, Muted, 13.0f);
        chip(canvas, chinese ? L"控件" : L"Controls", Rect{264.0f, 346.0f, 82.0f, 24.0f}, AccentSoft, Accent);
        chip(canvas, chinese ? L"布局" : L"Layouts", Rect{360.0f, 346.0f, 76.0f, 24.0f}, VioletSoft, Violet);
        chip(canvas, chinese ? L"数据" : L"Data", Rect{450.0f, 346.0f, 58.0f, 24.0f}, GreenSoft, Green);
        label(canvas, chinese ? L"使用侧边栏切换页面。" : L"Use the sidebar to switch sections.", Rect{264.0f, 400.0f, 360.0f, 20.0f}, Quiet, 12.0f);
    } else if (selectedSection_ == Section1) {
        card(canvas, Rect{ContentX, ContentY, 384.0f, ContentH});
        label(canvas, chinese ? L"控件样例" : L"Control Samples", Rect{InnerX, InnerY, 180.0f, 22.0f}, Ink, 17.0f);
        label(canvas, chinese ? L"属性、状态、事件和选择控件。" : L"Props, state, events, and selection controls.", Rect{InnerX, InnerY + 20.0f, 300.0f, 14.0f}, Muted, 12.0f);
        card(canvas, Rect{648.0f, ContentY, 346.0f, ContentH});
        label(canvas, chinese ? L"\u72b6\u6001\u8986\u76d6" : L"State Coverage", Rect{668.0f, InnerY, 180.0f, 22.0f}, Ink, 17.0f);
        label(canvas, chinese ? L"\u5b9e\u4f8b\u63a7\u4ef6\u76f4\u63a5\u5c55\u793a\u53ef\u4ea4\u4e92\u3001\u7981\u7528\u3001\u9009\u4e2d\u548c\u6821\u9a8c\u72b6\u6001\u3002" : L"Live controls cover interactive, disabled, selected, and validation states.", Rect{668.0f, InnerY + 26.0f, 286.0f, 32.0f}, Muted, 12.0f);
        chip(canvas, chinese ? L"\u6b63\u5e38 TextField" : L"Normal TextField", Rect{668.0f, 226.0f, 126.0f, 24.0f}, AccentSoft, Accent);
        chip(canvas, chinese ? L"\u5bc6\u7801 TextField" : L"Password TextField", Rect{806.0f, 226.0f, 144.0f, 24.0f}, VioletSoft, Violet);
        chip(canvas, chinese ? L"\u53ef\u4ea4\u4e92 Select" : L"Interactive Select", Rect{668.0f, 260.0f, 126.0f, 24.0f}, GreenSoft, Green);
        chip(canvas, chinese ? L"\u6821\u9a8c\u9519\u8bef" : L"Validation error", Rect{796.0f, 260.0f, 122.0f, 24.0f}, AmberSoft, Amber);
        label(canvas, chinese ? L"\u5bc6\u7801\u6a21\u5f0f\u53ea\u906e\u76d6\u663e\u793a\uff0c\u526a\u8d34\u677f\u4ecd\u4f7f\u7528\u771f\u5b9e\u503c\u3002" : L"Password mode masks display while clipboard keeps the real value.", Rect{668.0f, 286.0f, 306.0f, 18.0f}, Quiet, 12.0f);
        label(canvas, chinese ? L"\u53d1\u5e03\u901a\u9053 / \u5916\u90e8\u6309\u94ae" : L"Release channel / outside button", Rect{668.0f, 336.0f, 260.0f, 16.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"再打开下方 Select，或点击按钮；前一个 Select 会关闭，点击仍继续派发。" : L"Open this Select, then click the button or another Select; the first closes and the click continues.", Rect{668.0f, 392.0f, 306.0f, 34.0f}, Quiet, 12.0f);
        label(canvas, selectDispatchNote_.get(), Rect{668.0f, 434.0f, 306.0f, 34.0f}, Accent, 12.0f);
        label(canvas, chinese ? L"\u5c1d\u8bd5\u628a project key \u6539\u4e3a one-ui\uff1a\u9519\u8bef\u4f1a\u6d88\u5931\u3002" : L"Edit project key to one-ui: the error clears.", Rect{668.0f, 488.0f, 286.0f, 18.0f}, Quiet, 12.0f);
    } else if (selectedSection_ == Section2) {
        card(canvas, Rect{ContentX, ContentY, ContentW, 320.0f});
        label(canvas, chinese ? L"数据展示" : L"Data Display", Rect{InnerX, InnerY, 190.0f, 24.0f}, Ink, 17.0f);
        chip(canvas, L"MVP", Rect{924.0f, InnerY, 48.0f, 22.0f}, AccentSoft, Accent);
        label(canvas, chinese ? L"状态、可选择记录和小型表格。" : L"Statuses, selectable records, and small tables.", Rect{InnerX, InnerY + 26.0f, 360.0f, 18.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"状态" : L"Statuses", Rect{672.0f, 184.0f, 120.0f, 16.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"记录" : L"Records", Rect{672.0f, 246.0f, 120.0f, 16.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"运行" : L"Runs", Rect{812.0f, 246.0f, 120.0f, 16.0f}, Muted, 12.0f);
    } else if (selectedSection_ == Section3) {
        card(canvas, Rect{ContentX, ContentY, ContentW, 360.0f});
        label(canvas, chinese ? L"布局骨架" : L"Layout Skeletons", Rect{InnerX, InnerY, 190.0f, 24.0f}, Ink, 17.0f);
        chip(canvas, L"MVP", Rect{924.0f, InnerY, 48.0f, 22.0f}, AccentSoft, Accent);
        label(canvas, chinese ? L"滚动视图" : L"ScrollView", Rect{264.0f, 194.0f, 160.0f, 18.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"滚轮浏览长内容；拖动底部滑块查看宽内容右侧状态。" : L"Wheel through long content; drag the bottom thumb to inspect wide rows.", Rect{264.0f, 464.0f, 360.0f, 18.0f}, Quiet, 12.0f);
        label(canvas, L"DockView app shell", Rect{672.0f, 186.0f, 180.0f, 18.0f}, Muted, 12.0f);
        label(canvas, L"Wrap chip/tool row", Rect{672.0f, 292.0f, 180.0f, 18.0f}, Muted, 12.0f);
        label(canvas, L"SplitView content + inspector", Rect{672.0f, 366.0f, 220.0f, 18.0f}, Muted, 12.0f);
    } else if (selectedSection_ == Section4) {
        card(canvas, Rect{ContentX, ContentY, ContentW, ContentH});
        label(canvas, chinese ? L"样式系统 MVP" : L"Style System MVP", Rect{InnerX, InnerY, 220.0f, 24.0f}, Ink, 17.0f);
        label(canvas, chinese ? L"主题默认值、状态覆盖和组件级 override 的解析样例。" : L"Theme defaults, state overrides, and component-level style overrides.", Rect{InnerX, InnerY + 28.0f, 540.0f, 18.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"按钮状态" : L"Button states", Rect{InnerX, InnerY + 66.0f, 160.0f, 18.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"复选框状态" : L"Checkbox states", Rect{584.0f, InnerY + 66.0f, 170.0f, 18.0f}, Muted, 12.0f);
        label(canvas, chinese ? L"单选组样式" : L"RadioGroup override", Rect{764.0f, InnerY + 66.0f, 180.0f, 18.0f}, Muted, 12.0f);

        const std::array<const wchar_t*, 5> stateNames = chinese
            ? std::array<const wchar_t*, 5>{L"默认", L"悬停", L"按下", L"禁用", L"键盘焦点"}
            : std::array<const wchar_t*, 5>{L"Default", L"Hover", L"Pressed", L"Disabled", L"Focus"};
        const std::array<Color, 5> buttonBg{Accent, AccentHover, AccentPressed, Color{238, 241, 245}, Accent};
        const std::array<Color, 5> buttonFg{colors::White, colors::White, colors::White, Quiet, colors::White};
        const std::array<Color, 5> buttonBorder{Accent, AccentHover, AccentPressed, Color{226, 232, 240}, Accent};
        for (std::size_t i = 0; i < stateNames.size(); ++i) {
            const float y = 254.0f + static_cast<float>(i) * 34.0f;
            label(canvas, stateNames[i], Rect{InnerX, y, 88.0f, 26.0f}, Quiet, 12.0f);
            buttonSpecimen(canvas, Rect{362.0f, y, 126.0f, 26.0f}, stateNames[i], buttonBg[i], buttonFg[i], buttonBorder[i], i == 4 && focusRings_.get());
            checkboxSpecimen(canvas, Rect{584.0f, y, 160.0f, 26.0f}, i == 2 || i == 4, i == 3, i == 4 && focusRings_.get());
        }

        label(canvas, chinese ? L"解析顺序：主题默认 -> 状态默认 -> 组件覆盖 -> 焦点环可见性。" : L"Resolution: theme default -> state default -> component override -> focus-ring visibility.", Rect{InnerX, 442.0f, 650.0f, 18.0f}, Quiet, 12.0f);
        chip(canvas, L"CSS-like", Rect{826.0f, InnerY, 70.0f, 22.0f}, AccentSoft, Accent);
        chip(canvas, L"MVVM", Rect{906.0f, InnerY, 58.0f, 22.0f}, GreenSoft, Green);
    } else if (selectedSection_ == Section5) {
        card(canvas, Rect{ContentX, ContentY, ContentW, ContentH});
        label(canvas, chinese ? L"浮层模式对照" : L"Overlay mode comparison", Rect{InnerX, InnerY, 240.0f, 24.0f}, Ink, 17.0f);
        label(canvas, chinese ? L"三种 PopupInteractionMode 使用同样的内容，只改变外部点击、背景命中和焦点约束。" : L"The three PopupInteractionMode samples share content; only outside-click, background hit-test, and focus boundaries differ.", Rect{InnerX, InnerY + 28.0f, 660.0f, 18.0f}, Muted, 12.0f);

        const std::array<Rect, 3> columns{
            Rect{264.0f, 202.0f, 208.0f, 178.0f},
            Rect{504.0f, 202.0f, 208.0f, 178.0f},
            Rect{744.0f, 202.0f, 208.0f, 178.0f},
        };
        const std::array<const wchar_t*, 3> titles = chinese
            ? std::array<const wchar_t*, 3>{L"Modeless", L"LightDismiss", L"Modal"}
            : std::array<const wchar_t*, 3>{L"Modeless", L"LightDismiss", L"Modal"};
        const std::array<const wchar_t*, 3> descriptions = chinese
            ? std::array<const wchar_t*, 3>{L"背景仍可操作", L"点击外部关闭", L"阻止背景点击"}
            : std::array<const wchar_t*, 3>{L"Background remains active", L"Outside click dismisses", L"Background click is blocked"};
        const std::array<bool, 3> openStates{popupOpen_.get(), menuOpen_.get(), controlledPopupOpen_.get()};
        for (std::size_t i = 0; i < columns.size(); ++i) {
            card(canvas, columns[i]);
            label(canvas, titles[i], Rect{columns[i].x + 16.0f, columns[i].y + 14.0f, columns[i].width - 32.0f, 20.0f}, Ink, 15.0f);
            label(canvas, descriptions[i], Rect{columns[i].x + 16.0f, columns[i].y + 40.0f, columns[i].width - 32.0f, 18.0f}, Muted, 12.0f);
            chip(canvas,
                 openStates[i] ? (chinese ? L"已打开" : L"Open") : (chinese ? L"已关闭" : L"Closed"),
                 Rect{columns[i].x + 16.0f, columns[i].y + 116.0f, 70.0f, 22.0f},
                 openStates[i] ? GreenSoft : Color{238, 241, 245},
                 openStates[i] ? Green : Quiet);
        }

        popupTrigger(canvas, Rect{284.0f, 226.0f, 168.0f, 30.0f}, chinese ? L"打开 Modeless" : L"Open Modeless", popupOpen_.get());
        popupTrigger(canvas, Rect{524.0f, 226.0f, 168.0f, 30.0f}, chinese ? L"打开 LightDismiss" : L"Open LightDismiss", menuOpen_.get());
        popupTrigger(canvas, Rect{764.0f, 226.0f, 168.0f, 30.0f}, chinese ? L"打开 Modal" : L"Open Modal", controlledPopupOpen_.get());

        popupTrigger(canvas, Rect{284.0f, 414.0f, 190.0f, 30.0f}, chinese ? L"背景操作" : L"Background action", false);
        label(canvas,
              chinese ? L"用于验证 Modeless 允许背景操作；Modal 会阻止这里的点击。" : L"Use this to verify Modeless allows background work; Modal blocks it.",
              Rect{284.0f, 452.0f, 310.0f, 34.0f},
              Quiet,
              12.0f);
        chip(canvas,
             chinese ? (L"背景点击 " + std::to_wstring(backgroundClickCount_) + L" 次") : (L"Background clicks " + std::to_wstring(backgroundClickCount_)),
             Rect{284.0f, 494.0f, 138.0f, 22.0f},
             AccentSoft,
             Accent);

        popupTrigger(canvas, Rect{524.0f, 414.0f, 168.0f, 30.0f}, chinese ? L"定位示意" : L"Placement hint", false);
        const PopupPreferredPlacement activePlacement = popupPlacementByIndex(popupPlacement_);
        placementDiagram(canvas, Rect{524.0f, 486.0f, 186.0f, 74.0f}, activePlacement);
        chip(canvas, popupPlacementLabel(activePlacement), Rect{524.0f, 520.0f, 104.0f, 22.0f}, VioletSoft, Violet);
        label(canvas,
              chinese ? L"循环 bottom/top/left/right，观察主轴 flip 与可见区域 clamp。" : L"Cycle bottom/top/left/right and watch flip plus viewport clamp.",
              Rect{524.0f, 452.0f, 190.0f, 34.0f},
              Quiet,
              12.0f);

        popupTrigger(canvas, Rect{764.0f, 414.0f, 168.0f, 30.0f}, chinese ? L"禁用触发器" : L"Disabled trigger", false, popupDisabled_);
        label(canvas,
              chinese ? L"禁用触发器不会打开任何浮层。" : L"Disabled triggers do not open a popup.",
              Rect{764.0f, 452.0f, 184.0f, 34.0f},
              Quiet,
              12.0f);

        label(canvas, popupEventNote_, Rect{264.0f, 532.0f, 650.0f, 18.0f}, Quiet, 12.0f);
    }

    View::paint(canvas);
}

bool GalleryView::onMouseMove(const MouseEvent& event) {
    const int nextHover = hitTarget(event.position);
    const bool childChanged = View::onMouseMove(event);
    if (nextHover == hoveredTarget_) {
        return childChanged;
    }

    hoveredTarget_ = nextHover;
    return true;
}

bool GalleryView::onMouseDown(const MouseEvent& event) {
    const int target = hitTarget(event.position);
    if (selectedSection_ == Section5 && target < 0) {
        if (View::onMouseDown(event)) {
            return true;
        }

        const int overlayTarget = hitOverlayDemoTarget(event.position);
        if (overlayTarget == OverlayPlacementAction || overlayTarget == OverlayDisabledAction || overlayTarget == OverlayBackgroundAction) {
            pressedTarget_ = overlayTarget;
            focusedTarget_ = Section5;
            focusChild(nullptr);
            return true;
        }

        if (Rect{ContentX, ContentY, ContentW, ContentH}.contains(event.position)
            && (popupOpen_.get() || menuOpen_.get() || controlledPopupOpen_.get())) {
            if (controlledPopupOpen_.get()) {
                popupEventNote_ = chinese_.get()
                    ? L"模态浮层阻止了背景点击。请先关闭 Modal。"
                    : L"Background click blocked by the modal popup. Close Modal first.";
                invalidate();
                return true;
            }
            const bool dismissedLightDismiss = menuOpen_.get();
            if (menuOpen_.get()) {
                menuOpen_.set(false);
            }
            popupEventNote_ = chinese_.get()
                ? L"外部点击已关闭 LightDismiss；Modeless 仍保持打开。"
                : L"Outside click dismissed LightDismiss; Modeless remains open.";
            invalidate();
            return dismissedLightDismiss;
        }
    }

    if (target < 0) {
        return View::onMouseDown(event);
    }

    pressedTarget_ = target;
    focusedTarget_ = target;
    focusChild(nullptr);
    return true;
}

bool GalleryView::onMouseUp(const MouseEvent& event) {
    const int previousPressed = pressedTarget_;
    pressedTarget_ = -1;

    if (previousPressed < 0) {
        return View::onMouseUp(event);
    }

    if (selectedSection_ == Section5 && previousPressed >= OverlayBasicAction) {
        if (previousPressed == hitOverlayDemoTarget(event.position)) {
            if (previousPressed == OverlayBasicAction) {
                popupOpen_.set(!popupOpen_.get());
                popupEventNote_ = popupOpen_.get()
                    ? (chinese_.get() ? L"Modeless 已打开：背景按钮仍可响应。" : L"Modeless is open: the background action still responds.")
                    : (chinese_.get() ? L"Modeless 已关闭。" : L"Modeless closed.");
            } else if (previousPressed == OverlayPlacementAction) {
                popupPlacement_ = (popupPlacement_ + 1) % 6;
                popupEventNote_ = chinese_.get()
                    ? L"定位示意已切换：PopupPlacement 会按主轴 flip，再 shift/clamp 到可见区域。"
                    : L"Placement hint switched: PopupPlacement flips on the primary axis, then shifts/clamps into view.";
            } else if (previousPressed == OverlayMenuAction) {
                menuOpen_.set(!menuOpen_.get());
                popupEventNote_ = menuOpen_.get()
                    ? (chinese_.get() ? L"LightDismiss 已打开：点击外部会关闭它。" : L"LightDismiss is open: outside click will dismiss it.")
                    : (chinese_.get() ? L"LightDismiss 已关闭。" : L"LightDismiss closed.");
            } else if (previousPressed == OverlayControlledAction) {
                controlledPopupOpen_.set(!controlledPopupOpen_.get());
                popupEventNote_ = controlledPopupOpen_.get()
                    ? (chinese_.get() ? L"Modal 已打开：背景点击会被阻止。" : L"Modal is open: background clicks are blocked.")
                    : (chinese_.get() ? L"Modal 已关闭。" : L"Modal closed.");
            } else if (previousPressed == OverlayBackgroundAction) {
                ++backgroundClickCount_;
                popupEventNote_ = chinese_.get()
                    ? L"背景操作已响应。Modeless 打开时这仍然有效。"
                    : L"Background action responded. This still works while Modeless is open.";
            } else if (previousPressed == OverlayDisabledAction) {
                popupOpen_.set(false);
                popupEventNote_ = chinese_.get()
                    ? L"\u7981\u7528\u89e6\u53d1\u5668\u4e0d\u4f1a\u6253\u5f00 Popup\u3002"
                    : L"Disabled trigger did not open a popup.";
            }
            invalidate();
        }
        return true;
    }

    if (previousPressed == hitTarget(event.position)) {
        if (previousPressed >= Section0 && previousPressed <= Section5) {
            selectSection(previousPressed);
        } else if (previousPressed == CreateAction) {
            selectSection(Section0);
        } else if (previousPressed == PreviewAction) {
            const bool nextChinese = !chinese_.get();
            chinese_.set(nextChinese);
            selectDispatchNote_.set(nextChinese
                ? L"打开一个 Select，再点击按钮或另一个 Select。"
                : L"Open a Select, then click the button or another Select.");
            invalidate();
        } else if (previousPressed == ExportAction) {
            selectSection(Section4);
        }
    }

    return previousPressed >= 0;
}

bool GalleryView::onKeyDown(const KeyEvent& event) {
    if (View::onKeyDown(event)) {
        return true;
    }

    if (selectedSection_ == Section5 && event.key == Key::Escape
        && (popupOpen_.get() || menuOpen_.get() || controlledPopupOpen_.get())) {
        const bool closedModal = controlledPopupOpen_.get();
        const bool closedLightDismiss = menuOpen_.get();
        const bool closedModeless = popupOpen_.get();
        popupOpen_.set(false);
        menuOpen_.set(false);
        controlledPopupOpen_.set(false);
        popupEventNote_ = chinese_.get()
            ? L"Escape 已关闭当前打开的浮层。"
            : L"Escape closed the open popup modes.";
        if (closedModal || closedLightDismiss || closedModeless) {
            invalidate();
        }
        return true;
    }

    if (event.key == Key::Tab) {
        focusedTarget_ += event.shift ? -1 : 1;
        if (focusedTarget_ < 0) {
            focusedTarget_ = TargetCount - 1;
        } else if (focusedTarget_ >= TargetCount) {
            focusedTarget_ = 0;
        }
        return true;
    }

    if (event.key == Key::Enter || event.key == Key::Space) {
        if (focusedTarget_ >= Section0 && focusedTarget_ <= Section5) {
            selectSection(focusedTarget_);
        } else if (focusedTarget_ == CreateAction) {
            selectSection(Section0);
        } else if (focusedTarget_ == PreviewAction) {
            const bool nextChinese = !chinese_.get();
            chinese_.set(nextChinese);
            selectDispatchNote_.set(nextChinese
                ? L"打开一个 Select，再点击按钮或另一个 Select。"
                : L"Open a Select, then click the button or another Select.");
            invalidate();
        } else if (focusedTarget_ == ExportAction) {
            selectSection(Section4);
        }
        return true;
    }

    return false;
}

bool GalleryView::onFocusChanged(bool focused) {
    hasWindowFocus_ = focused;
    View::onFocusChanged(focused);
    return true;
}

} // namespace oneui::gallery
