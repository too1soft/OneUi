# OneUI UI 红线：禁止魔改和组件特判

日期：2026-05-28

本文是 OneUI 和所有下游产品，包括 Remote、网云穿、iShell 的硬性开发红线。它不是建议，不是风格偏好，而是代码合入前必须满足的工程契约。

## 核心原则

OneUI 是通用 UI 框架。框架层只能实现通用机制，不能为了某个页面、某个组件、某个坐标范围、某个产品截图效果写特殊分支。

Remote 是下游产品。Remote 只能组合 OneUI 组件、传入文案/数据/事件回调，不能在业务工程里复制、绕开或修补 OneUI 的通用视觉和交互能力。

## 绝对禁止

1. 禁止按固定坐标或固定宽度做 UI 特判。

   反例：

   ```cpp
   if (rect.x < 220) {
       repaint_sidebar();
   }
   ```

   这类代码属于魔改。即使它能临时缓解 hover 卡顿，也必须删除。正确做法是修复 dirty rect 传播、命中测试、局部重绘、动画调度或容器无效区域合并机制。

2. 禁止按组件名称做渲染后门。

   反例：

   ```cpp
   if (widget->className() == "sidebar") {
       forceFullHeightInvalidate();
   }
   ```

   正确做法是给所有 `Widget`/`View` 提供统一的 invalidation、clip、paint order、state transition 能力。

3. 禁止按产品名称或业务名词做 OneUI 内部分支。

   反例：

   ```cpp
   if (product == "Remote") {
       useRemoteHoverPath();
   }
   ```

   OneUI 里不能出现 Remote 首页专属逻辑。可以出现通用组件名，如 `ProductShell`、`NavItem`、`TextField`、`StatusStrip`，但不能出现设备码、验证码、远程控制等业务规则。

4. 禁止在 Remote 中补通用控件行为。

   Remote 不得自己实现按钮颜色、输入框 caret、hover/focus/pressed/disabled/read-only、阴影、边框、滚动条、标题栏、窗口 resize/fullscreen 动画等通用行为。缺什么先补 OneUI，再在 Remote 使用。

5. 禁止“为了性能”绕过标准架构。

   性能优化必须落到通用机制：

   - dirty rect 精准传播
   - 局部 repaint
   - paint surface 缓存
   - clip-based painting
   - 事件合并
   - animation frame 调度
   - hover state diff
   - layout cache
   - text metrics cache

   不能落到“某个区域全量刷新”“某个控件特殊刷新”“某个页面特殊跳过”的魔改。

## 允许的做法

允许使用公开参数、样式 token、组件 metrics 和通用配置来表达不同产品需求。

示例：

```cpp
struct ProductShellMetrics {
    float sidebarWidth = 184.0f;
    float topBarHeight = 48.0f;
    float compactBreakpoint = 720.0f;
};
```

这是通用布局参数，可以被不同产品配置。

不允许：

```cpp
if (dirty.x < 184) {
    repaintWholeSidebar();
}
```

这是按某个布局区域写死的渲染后门。

## 性能问题的标准处理路径

当 UI 卡顿、hover 延迟、光标不闪、全屏闪烁时，必须按下面顺序定位：

1. 事件是否到达正确 widget。
2. hover/focus/pressed 状态是否只在实际变化时更新。
3. `invalidateRect` 是否沿子树正确传播到窗口。
4. dirty rect 坐标系是否一致。
5. `paint` 是否遵守 clip，是否只画 dirty 区域。
6. animation frame 是否只在有动画时继续调度。
7. 文本测量、阴影、渐变等昂贵操作是否有缓存。
8. resize/fullscreen 是否避免 erase background 和旧帧闪烁。
9. smoke/golden screenshot 是否覆盖该行为。

以上步骤没有完成前，禁止加入任何坐标特判或产品特判。

## 合入检查清单

每次改 OneUI 或 Remote UI 前必须回答：

1. 这个问题是通用 UI 能力缺陷，还是 Remote 业务问题？
2. 如果是通用 UI 缺陷，是否已经在 OneUI 修复？
3. 是否出现了固定坐标、固定产品尺寸、固定组件名的特殊分支？
4. 是否出现了下游产品名或业务名词进入 OneUI 核心？
5. 是否有行为测试、截图烟测或红线扫描覆盖？
6. 是否能解释这个修复对其他产品同样成立？

只要第 3 或第 4 条为“是”，该实现必须退回。

## 当前明确红线案例

`requestRedrawRect` 里根据 `rect.x < 220` 重绘整条侧边栏，是明确禁止的魔改。正确修复方向是：

- 修复容器和子控件的 `setRectInvalidator` 传播。
- 修复 dirty rect 坐标和 clip。
- 修复 hover state diff。
- 修复 `WM_PAINT` 局部 blit。
- 增加 hover/input/resize smoke。

不得恢复这个分支，不得换一个数字继续特判。

## 自动化要求

OneUI 和 Remote UI 相关变更必须运行红线扫描。红线扫描不是完整代码审查，但可以拦截最危险的魔改模式。

```powershell
powershell -ExecutionPolicy Bypass -File E:\project\byname\oneui\scripts\check-ui-red-lines.ps1
```

如果扫描报错，必须修复代码或在文档中明确说明误报原因，并把误报规则收敛到更准确，而不是绕过脚本。

## 性能红线：新组件必须默认可流畅交互

OneUI 是公共 UI 框架。任何新组件、布局、窗口能力、绘制 primitive 合入前，都必须把性能视为基础功能，而不是后期优化项。

### 禁止事项

1. 禁止在鼠标移动、hover、键盘输入、caret 闪烁、scroll、resize 等高频事件中同步执行重绘。
   高频事件只允许更新状态、标记 dirty rect、提交统一调度。不得在事件处理函数中直接制造长耗时 paint。

2. 禁止新增未缓存的昂贵 primitive。
   下列操作如果在每帧或每个控件重复执行，必须有通用缓存或明确的性能说明：
   - 阴影 blur
   - 渐变 raster
   - 文本 blob / glyph layout
   - 文本测量
   - 图片缩放
   - 大面积圆角裁剪
   - filter / mask / shader 创建

3. 禁止 hover/focus/pressed 状态导致整窗重绘。
   交互状态变化必须优先走局部 invalidation，并且 dirty rect 必须由组件树标准传播，不能用固定坐标扩大重绘区域。

4. 禁止 resize/fullscreen 产生多段可见中间状态。
   窗口状态切换必须尽量用一次原子化平台操作完成。borderless 窗口不得无意义地反复改 style、移动窗口、再 resize。

5. 禁止把性能问题归因于 Debug、CPU、GPU 或用户机器，除非 trace 已经证明瓶颈不在 OneUI。
   Debug 版本可以慢，但不能因为一个普通桌面 UI 的 hover、输入、resize 就出现明显阻塞。

6. 禁止“为了赶进度先不做缓存”。
   如果组件引入昂贵绘制能力，缓存策略、失效策略和基本 smoke 必须随组件一起提交。

### 必须事项

1. 新组件必须支持状态矩阵下的局部重绘：
   - normal
   - hover
   - pressed
   - focus
   - disabled
   - selected
   - read-only

2. 新组件必须遵守统一调度：
   - 事件处理不直接同步重绘
   - animation frame 只在有动画时持续调度
   - caret blink 只重绘 caret 相关区域
   - hover 只在状态实际变化时触发 invalidation

3. 新组件如果使用阴影、渐变、文本、图标、图片，必须复用 OneUI 的公共 primitive 和缓存层。
   不允许组件内部私自复制一套 shadow cache、gradient cache、text cache。

4. resize/fullscreen/restore 相关改动必须至少验证：
   - 不擦背景
   - 不出现白屏或旧帧闪烁
   - 不为每个尺寸消息强制同步满屏重绘
   - 连续 resize 不出现明显掉帧

5. 性能问题必须先开 trace 再下结论。
   推荐最小 trace 维度：
   - `avg_paint`
   - `avg_content`
   - `avg_blit`
   - `surface_alloc_ms`
   - `text`
   - `measure`
   - `shadow`
   - `gradient`

### 参考案例

Remote 首页曾出现拖拽 resize 卡顿、全屏闪烁、hover 不跟手。最终定位为 OneUI 通用渲染管线问题，而不是 Remote 业务代码问题。

经典案例文档：

`E:\project\byname\oneui\docs\27-performance-case-remote-rendering.md`

## 新组件性能准入红线

以后任何 OneUI 新组件、新 primitive、新窗口能力、新布局能力合入前，都必须默认满足以下性能准入要求。性能不是 Remote 发现卡顿后再补救的事情，而是 OneUI 公共组件设计的一部分。

1. 必须先确认高频路径是否无阻塞。
   `WM_MOUSEMOVE`、hover、focus、caret、keyboard input、resize、scroll、animation tick 这些路径不能同步执行重绘、测量整棵树、重建大对象、同步 flush paint。

2. 必须有标准 dirty rect。
   状态变化只能让实际变化的组件区域失效。禁止为了省事扩大到整窗、整页、整条侧边栏，也禁止写任何坐标阈值、产品阈值、组件名特判。

3. 必须使用 OneUI 通用缓存。
   阴影、渐变、文本 blob、文本测量、图片缩放、mask/filter/shader 创建都必须进入 OneUI 通用 primitive/cache 层。组件内部不能私自做一套缓存，更不能每帧重新创建昂贵对象。

4. 必须用 trace 定位性能问题。
   新组件造成卡顿、闪烁、hover 不跟手、resize 掉帧时，先打开 `ONEUI_RENDER_TRACE=1`，看 `avg_paint`、`avg_content`、`avg_blit`、`surface_alloc_ms`、`text`、`measure`、`shadow`、`gradient`，禁止靠猜修复。

5. 必须保护 Debug 体验。
   Debug 可以慢，但不能让普通桌面 UI 的 hover、输入、resize 出现明显阻塞。不能用“Release 会好”作为合入理由。

6. 必须保护窗口状态切换。
   最大化、还原、全屏、退出全屏、拖动 resize 不能出现白屏、旧帧闪烁、先移动再缩放的可见中间态。窗口 backend 必须优先使用原子化平台操作和统一调度。

7. 必须把修复沉淀在 OneUI。
   如果性能问题属于通用 UI 能力，修复只能进入 OneUI 的 scheduler、backend、primitive、layout、style、invalidation 体系。Remote、网云穿、iShell 等下游项目不得写绕路补丁。

该案例形成以下硬性经验：

- 先加 trace，不靠猜。
- 先区分 content、blit、surface allocation。
- 再拆 text、shadow、gradient、measure。
- 在 OneUI primitive、调度器、dirty rect、窗口 backend 修。
- 用 smoke 和 resize 压测验证。
- 不在下游业务项目写临时优化分支。
