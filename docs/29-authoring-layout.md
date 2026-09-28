# 样式与布局第一阶段

> 后续进展：第二阶段已补齐 C ABI / Rust 布局包装、统一内容测量和组合式页面 API，见[组合布局与方案研究](30-composition-and-layout-research.md)。下文保留第一阶段的实现范围。

本次补齐 C++ 控件绘制和布局能力，不替换渲染后端。现有 C ABI / Rust 的 stylesheet 路径会自动获得按钮 CSS padding 修复；新增的布局和测量接口目前只在 C++ 层，尚未新增 C ABI / Rust 包装。

## 按钮 padding 与内容测量

`ButtonStyle`、`ButtonStateStyleOverride` 新增可选 `padding`。CSS `padding` 经 style adapter 传入实际绘制，支持普通、悬停、按下等状态。未设置时保留旧按钮行为；显式 `padding: 0` 清除隐含内容缩进。背景、边框、点击区域不缩小；文字、图标和尾部信息使用内边距后的区域，并裁剪到该区域。过大的 padding 留下空内容区域，不产生负尺寸。

```cpp
StyleSheet sheet;
std::string error;
if (!sheet.addRulesFromCss(
        "button { padding: 8px 16px; font-size: 14px; }", &error)) {
    throw std::runtime_error(error);
}
button->setStyleOverride(buttonStyleOverrideFromStyleSheet(
    sheet, StyleNode{"button", {}, StyleStateNone}));
auto size = button->naturalContentSize();
button->setPreferredSize({std::ceil(size.width), std::max(36.0f, size.height)});
```

`naturalContentSize()` 使用当前解析样式、文字、图标、尾部文字和字体环境计算尺寸，不自动修改 preferredSize。请在窗口传播字体/DPI **之后** 测量，并在文字、样式、字体变化后重新测量。按下或悬停通常只改颜色，避免切换状态时尺寸跳动。无显式 padding 的测量为左右各预留 12；没有统一的递归 intrinsic layout 协议。

## Stack 的主轴分配

```cpp
auto row = std::make_shared<Stack>(StackDirection::Row);
row->setGap(8);
row->setAlign(StackAlign::Center);
row->add(content);
row->add(actions);
row->setFlex(content, StackFlex{1, 1, 240, 160, 600});
//                              grow shrink basis min max
row->setJustify(StackJustify::SpaceBetween);
```

- 不调用 `setFlex`：旧逻辑不变，正 preferredSize 固定，非正值平分剩余空间，不主动压缩固定项。
- 显式 flex：默认 grow=0、shrink=1、basis=preferredSize、min=0、max=无穷。
- 多余空间按 grow 比例分配；不足时按 shrink × basis 分配压缩量。到达 min/max 后冻结该项，将余额继续分给其余项。
- min 无法同时满足时允许溢出，不偷偷违反最小尺寸。需要应用设置最小窗口尺寸或滚动容器。
- `Start`、`Center`、`End`、`SpaceBetween` 仅分配伸缩后剩余的正空间。gap 是最小间距，隐藏项不占空间或 gap。
- `clearFlex(child)` 恢复旧逻辑。配置使用弱引用，不延长控件生命周期；布局临时数组复用容量。
- `contentWidth/Height` 是按 basis/约束计算的首选内容范围，不是伸缩后的实际子项外包围框。

这是一维布局能力，不是完整 CSS Flexbox：没有 wrap、order、align-self，也未增加 CSS flex 属性解析。配置接口只针对该 Stack 的直接子项。

## 集成与验证

C++ 类和样式结构体布局有变化，DLL 与 C++ 使用方需要用同一份头文件重新编译；不能直接替换旧 C++ 程序旁的 DLL。现有 C ABI 结构体未改变。

新增 `oneui_authoring_behavior_tests` 验证 CSS 到绘制、状态 padding、零/超大 padding、内容测量、加权伸缩、边界再分配、隐藏项、主轴对齐、旧布局恢复。原 `oneui_control_behavior_tests`、`oneui_stack_behavior_tests` 同时回归。
