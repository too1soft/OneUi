# Overlay 与 Popup Placement

> 状态：已接受的窄 runtime slice。本文覆盖 `PopupPlacement` 的上下/左右定位、主轴翻转、shift/clamp，popup style token、`OverlayHost` 基础挂载/事件/焦点边界，以及 `Popup` 的外部指针策略、Escape 关闭和 modeless / light-dismiss / modal 薄预设。Gallery 的 Overlay/Popup 页面已经提供 Modeless / LightDismiss / Modal 三列对照示例。完整 Menu、Tooltip、Dialog、Toast 外观仍是后续 slice。

## 为什么需要它

桌面 UI 框架中很多控件都需要浮层：

- `Select` dropdown
- context menu
- tooltip
- popover
- dialog-like surface
- 表单字段旁的 validation hint

这些控件都需要同一个基础能力：给定 anchor rectangle、期望浮层尺寸和可见 viewport，计算一个不会跑出窗口的 popup rectangle。

`PopupPlacement` 是这个共享基础。它是纯 C++ resolver，不打开窗口、不绘制 UI、不捕获焦点，也不管理关闭逻辑。这样它容易测试，也能被 Select、Menu、Tooltip、Dialog 等后续控件复用。

## 3 分钟快速开始

```cpp
oneui::PopupPlacementRequest request{
    oneui::Rect{40.0f, 120.0f, 96.0f, 32.0f},   // anchor
    oneui::Size{180.0f, 96.0f},                  // preferredSize
    oneui::Rect{0.0f, 0.0f, 640.0f, 420.0f},     // viewport
    oneui::PopupPreferredPlacement::BottomStart, // preferredPlacement
    6.0f                                         // offset
};

oneui::PopupPlacementResult result = oneui::PopupPlacement::resolve(request);
```

返回值：

- `result.rect`：最终 popup rectangle。
- `result.placement`：实际使用的 placement，可能与首选值不同。
- `result.flipped`：是否在主轴方向发生翻转；上下 placement 在 top/bottom 之间翻转，左右 placement 在 left/right 之间翻转。

## PopupPreferredPlacement

```cpp
enum class PopupPreferredPlacement {
    BottomStart,
    BottomEnd,
    TopStart,
    TopEnd,
    LeftStart,
    RightStart
};
```

| 值 | 含义 |
| --- | --- |
| `BottomStart` | 浮层在 anchor 下方，左边缘与 anchor 左边缘对齐。 |
| `BottomEnd` | 浮层在 anchor 下方，右边缘与 anchor 右边缘对齐。 |
| `TopStart` | 浮层在 anchor 上方，左边缘与 anchor 左边缘对齐。 |
| `TopEnd` | 浮层在 anchor 上方，右边缘与 anchor 右边缘对齐。 |
| `LeftStart` | 浮层在 anchor 左侧，顶部与 anchor 顶部对齐。 |
| `RightStart` | 浮层在 anchor 右侧，顶部与 anchor 顶部对齐。 |

## PopupPlacementRequest

```cpp
struct PopupPlacementRequest {
    Rect anchor;
    Size preferredSize;
    Rect viewport;
    PopupPreferredPlacement preferredPlacement = PopupPreferredPlacement::BottomStart;
    float offset = 0.0f;
};
```

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `anchor` | 是 | 触发控件矩形，坐标系必须与 `viewport` 一致。 |
| `preferredSize` | 是 | 碰撞处理前期望的浮层尺寸。 |
| `viewport` | 是 | 浮层应保持在其中的可见区域。 |
| `preferredPlacement` | 否 | 优先尝试的定位方向，默认 `BottomStart`。 |
| `offset` | 否 | anchor 和 popup 之间的间距，默认 `0`。 |

## PopupPlacementResult

```cpp
struct PopupPlacementResult {
    Rect rect;
    PopupPreferredPlacement placement = PopupPreferredPlacement::BottomStart;
    bool flipped = false;
};
```

| 字段 | 说明 |
| --- | --- |
| `rect` | 经过 flip、shift 和 clamp 后的最终浮层矩形。 |
| `placement` | 实际使用的定位方向。 |
| `flipped` | resolver 在主轴方向切换时为 `true`，例如 `BottomStart -> TopStart` 或 `RightStart -> LeftStart`。 |

## 碰撞处理规则

`PopupPlacement::resolve(...)` 按以下顺序处理：

1. 根据 `anchor`、`preferredSize`、`preferredPlacement` 和 `offset` 生成首选矩形。
2. 如果首选矩形在主轴方向溢出，尝试相反方向：上下 placement 比较垂直溢出，左右 placement 比较水平溢出。
3. 选择主轴溢出更少的一侧。
4. 如果浮层宽高大于 viewport，先钳制到 viewport 尺寸。
5. 最后把 x/y 平移回 viewport 内。

注意：

- `BottomStart` / `BottomEnd` 会与对应的 `TopStart` / `TopEnd` 互相翻转。
- `LeftStart` / `RightStart` 会在左右两侧互相翻转。
- 非主轴方向的碰撞通过 shift 解决，例如 `BottomStart` 的水平溢出仍会向左或向右平移。
- 过大的浮层会被钳制到 viewport 大小。
- resolver 是确定性的，同样输入得到同样输出。

## 示例

### 正常 BottomStart

```cpp
auto result = oneui::PopupPlacement::resolve({
    oneui::Rect{20, 30, 80, 24},
    oneui::Size{120, 60},
    oneui::Rect{0, 0, 300, 220},
    oneui::PopupPreferredPlacement::BottomStart,
    6
});

// result.rect == {20, 60, 120, 60}
// result.placement == BottomStart
// result.flipped == false
```

解释：anchor 底部是 `30 + 24 = 54`，加上 `6` 的间距，所以 popup y 是 `60`。

### 从 Bottom 翻到 Top

```cpp
auto result = oneui::PopupPlacement::resolve({
    oneui::Rect{20, 170, 80, 24},
    oneui::Size{120, 60},
    oneui::Rect{0, 0, 300, 220},
    oneui::PopupPreferredPlacement::BottomStart,
    6
});

// result.rect == {20, 104, 120, 60}
// result.placement == TopStart
// result.flipped == true
```

解释：放在下方会超出 viewport，因此 resolver 翻到上方。上方 y 是 `170 - 60 - 6 = 104`。

### 右侧定位与翻转

```cpp
auto result = oneui::PopupPlacement::resolve({
    oneui::Rect{160, 40, 40, 20},
    oneui::Size{70, 50},
    oneui::Rect{0, 0, 220, 160},
    oneui::PopupPreferredPlacement::RightStart,
    8
});

// result.rect == {82, 40, 70, 50}
// result.placement == LeftStart
// result.flipped == true
```

解释：首选右侧时 x 会是 `160 + 40 + 8 = 208`，宽度 `70` 会超出 viewport 右边界，因此 resolver 改放到左侧，x 是 `160 - 70 - 8 = 82`。

### 平移回 viewport 内

```cpp
auto result = oneui::PopupPlacement::resolve({
    oneui::Rect{260, 30, 50, 24},
    oneui::Size{120, 60},
    oneui::Rect{0, 0, 300, 220},
    oneui::PopupPreferredPlacement::BottomStart,
    6
});

// result.rect == {180, 60, 120, 60}
```

解释：x 是 `260`，宽度是 `120`，右边缘会到 `380`，超过 `300` 宽的 viewport。resolver 把 x 左移到 `300 - 120 = 180`。

### 钳制过大 popup

```cpp
auto result = oneui::PopupPlacement::resolve({
    oneui::Rect{10, 10, 40, 20},
    oneui::Size{400, 260},
    oneui::Rect{5, 7, 180, 90},
    oneui::PopupPreferredPlacement::BottomEnd,
    4
});

// result.rect == {5, 7, 180, 90}
```

解释：期望浮层尺寸大于 viewport，所以宽高被钳制到 `180 x 90`，x/y 也被钳制到 viewport 起点。

## Style Token

`Theme::popup` 通过 `PopupStyle` 提供默认 token：

```cpp
struct PopupStyle {
    Color background;
    Color foreground;
    Color border;
    float borderWidth;
    float radius;
    Insets padding;
    float offset;
    float elevation;
    int layer;
};
```

这些字段是强类型 token，不是运行时 CSS 字符串。设计目标是保留接近 CSS 的自由度，同时保持 C++ 类型安全：

- 默认值放在 `Theme::popup`。
- 单个组件通过 `PopupStyleOverride` 覆盖。
- 定位逻辑读取 `offset` 这类强类型数值。
- 层级顺序使用数字 `layer` token，避免 scattered magic value。

示例：

```cpp
oneui::PopupStyleOverride style;
style.offset = 8.0f;
style.radius = 6.0f;
style.layer = 200;
```

## 外部点击与命中规则

`Popup` 打开后会先命中 anchor，再命中 content。外部区域是否被 `Popup` 捕获，由 `setOutsidePointerPolicy(...)` 决定：

```cpp
popup->setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::PassThrough);
popup->setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::Close);
popup->setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::Block);
```

- `PassThrough`：外部指针不会关闭当前 `Popup`，也不会被当前 `Popup` 吞掉。它在 `OverlayHost` 中会继续交给更低层、并且真正命中的 overlay 或 view 处理。
- `Close`：外部 mouse down 会关闭当前 `Popup` 并消费这次点击。这个行为适合 Select、Menu、Popover 这类点击外部自动收起的组件。
- `Block`：外部 mouse down / wheel 会被当前 `Popup` 消费，但不会关闭它。这个行为适合需要保持打开、同时不允许底层界面被误操作的浮层。

`setCloseOnOutsideClick(false)` 仍然可用，它等价于 `PassThrough`；`setCloseOnOutsideClick(true)` 等价于 `Close`。新代码建议直接使用 `setOutsidePointerPolicy(...)`，因为三态语义更清楚。

这条规则很重要：`PassThrough` 通常用于非模态浮层，它不应该挡住底层可点击区域；`Block` 则用于“保持打开但阻止底层误点”的场景。

## 交互模式预设

如果你只是要表达常见语义，不想手动记住 `trapsFocus`、`blocksOutsidePointer` 和外部点击策略，可以用 `PopupInteractionMode`：

```cpp
auto popup = std::make_shared<oneui::Popup>();
popup->setAnchor(button);
popup->setContent(panel);
popup->setOpen(true);

popup->setInteractionMode(oneui::PopupInteractionMode::Modal);
host->addOverlay(popup, popup->overlayOptions(oneui::theme().layerModal));
```

当前预设是很薄的一层，不会引入新的事件系统，只会映射到已经验收的底层能力：

| 模式 | 适合场景 | Popup 外部指针策略 | Overlay 焦点边界 | Overlay 外部指针阻断 |
| --- | --- | --- | --- | --- |
| `Modeless` | Tooltip、状态浮层、不会打断用户当前操作的辅助内容 | `PassThrough` | 否 | 否 |
| `LightDismiss` | 下拉菜单、Select、Popover：点外部应关闭，但不需要锁住焦点 | `Close` | 否 | 否 |
| `Modal` | Dialog、确认框、必须先完成或取消的浮层 | `Block` | 是 | 是 |

Gallery 的 Overlay/Popup 页面把这三种模式并排展示，便于直接比较 background click、outside dismiss 和 modal block 的差异：`Modeless` 不拦背景点击，`LightDismiss` 点外部关闭自己，`Modal` 阻止外部指针落到底层。

`modal` 不是“自动画遮罩”。它表示交互约束：键盘焦点留在浮层内，外部指针不会穿透到底层界面。遮罩颜色、动画、关闭按钮和具体 Dialog 外观仍然是后续组件层能力。

高级用法仍然可以直接使用底层 API：

```cpp
host->addOverlay(dialog, oneui::OverlayOptions::modal(oneui::theme().layerModal));
host->addOverlay(popover, oneui::OverlayOptions::modeless(oneui::theme().layerPopup));
popup->setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::Close);
```

不要只是为了“点击外部关闭”而使用 `Modal`。点击外部关闭是 `LightDismiss` 或 `PopupOutsidePointerPolicy::Close` 的语义；`Modal` 适合需要阻止用户操作底层界面的任务。

## 当前尚未完成

已接受的窄 slice：

- `PopupPlacement` 纯几何定位。
- `OverlayHost` 挂载、移除、层级、边界、基础事件转发、`OverlayOptions::trapsFocus` 键盘焦点边界、`OverlayOptions::blocksOutsidePointer` 外部指针阻断、移除当前聚焦 overlay 后的焦点降级、overlay-to-overlay 焦点历史恢复，以及最后一个聚焦 overlay 关闭后返回打开前的普通 View 焦点；如果下一层 overlay 接管焦点，普通 View 不会收到短暂的恢复焦点事件。
- `Popup` 的 anchor/content 命中、鼠标焦点交接、键盘转发、`open/bindOpen`、`PopupOutsidePointerPolicy` 三态外部指针策略、`PopupInteractionMode` 交互模式预设、`setCloseOnEscape(...)` Escape 关闭策略和基础阴影绘制。
- `Select` 仍是内置 dropdown，但 light-dismiss 行为基线已接受：点击 field/dropdown 外部会关闭，不改变 `selectedIndex`，不触发 `onChanged`；点击其它控件可继续派发；点击另一个 `Select` 会关闭前一个并打开后一个。
- Gallery 的 Overlay/Popup 页面包含 Modeless / LightDismiss / Modal 三列对照示例，用于观察 background click、outside dismiss、modal block 的行为差异。

仍未完成：

- 完整 modal 遮罩绘制、动画和 Dialog 组件外观。
- 动画。
- Popup 内部多子控件焦点循环。
- 完整网站组件页。

`OverlayHost` 基础行为已不只是计划：挂载、移除、z-order、bounds 和事件转发都有行为测试。建议后续顺序：

1. `Popup`：内部多子控件焦点循环和 Select 迁移前的组合行为。
2. Select 迁移：把当前内部 dropdown 替换到共享 overlay/popup 基础设施。
3. Gallery：继续补 placement、menu-like popup、controlled state 等更完整示例。
4. 网站/文档：runtime 行为接受后再补完整组件页。

## 测试

已接受测试覆盖：

- bottom-start placement and offset。
- bottom overflow flipping to top。
- horizontal viewport shift。
- oversized popup clamping。
- `OverlayHost` mount/remove、z-order、bounds、基础 event forwarding。
- `OverlayHost` 移除当前聚焦 overlay 后，会把焦点降级到下一层可聚焦 overlay，并保留 focus-visible 状态。
- `OverlayHost` 移除当前聚焦 overlay 时，会优先恢复到历史上的前一个可用 overlay；如果历史目标不可用，再降级到剩余可聚焦 overlay。
- `OverlayHost` 支持 `OverlayOptions{layer, trapsFocus}`；启用 `trapsFocus` 的 overlay 会把 Tab/Shift+Tab 的键盘焦点限制在该 overlay 及其上层 overlay 内，不会跳到底层 overlay 或普通 View 子控件。
- `OverlayHost` 支持 `OverlayOptions{layer, trapsFocus, blocksOutsidePointer}`；启用 `blocksOutsidePointer` 的 overlay 会消费自己外部、且位于自己下方的 mouse down/up/wheel，不让事件落到底层 overlay 或普通 View 子控件；它上方更高层 overlay 仍可正常命中。
- `OverlayHost` 移除最后一个聚焦 overlay 后，会把焦点还给打开 overlay 前的普通 View 子控件，并保留 focus-visible 状态。
- `OverlayHost` 在下一层 overlay 接管焦点时，不会让普通 View 子控件收到短暂的恢复焦点/失去焦点抖动。
- `Popup` outside hit-test respects `setCloseOnOutsideClick(...)`。
- `Select` 内置 dropdown 的 light-dismiss 行为基线：打开后点击 field/dropdown 外部会关闭，不改变 `selectedIndex`，不触发 `onChanged`；外部点击仍可继续派发给其它控件；点击另一个 `Select` 会关闭前一个并打开后一个。
- `PopupOutsidePointerPolicy::Block` 可以阻断外部 mouse down / wheel，但保持 `Popup` 打开；`PassThrough` 继续放行到底层，`Close` 继续关闭并消费外部点击。
- `PopupInteractionMode::Modal` 会把 `Popup` 的外部指针策略设为 `Block`，并通过 `popup->overlayOptions(...)` 生成带 `trapsFocus` 和 `blocksOutsidePointer` 的 `OverlayOptions`。
- 已聚焦的 `Popup` 能通过 `OverlayHost` 接收 Escape，并在 `setCloseOnEscape(true)` 时关闭。
- `Popup` 鼠标点击 anchor/content 时会交接内部焦点，并把键盘事件转发给当前内部焦点控件。

运行：

```powershell
cmake --build --preset ucrt64 --target oneui_control_behavior_tests
ctest --test-dir build\ucrt64 --output-on-failure -R oneui_control_behavior_tests
```

## FAQ

### 这已经是完整 Popup 组件了吗？

不是。`PopupPlacement` 只计算几何位置，不负责 open state、事件、焦点或绘制。

### Select 现在能直接迁移吗？

还不能作为共享 overlay 直接迁移。`Select` 当前有已接受的内部最小 dropdown、popup-state cleanup 和 light-dismiss 行为基线；迁移到共享 `OverlayHost` / `Popup` 基础设施仍是后续工作。

### 为什么不一次做完？

完整 Overlay/Popup 行为牵涉打开状态、外部点击、键盘关闭、焦点交接、层级、动画和 Gallery 展示。拆小后每一层都能单独测试，避免把半成品交互固化成 API debt。
