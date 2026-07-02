# OneUI 下游产品集成标准

日期：2026-05-25

`remote` 是 OneUI 的第一个重度真实下游产品。它不是一次性业务壳，而是 OneUI 是否能承载高性能桌面工具的验证场景。本文定义 OneUI 面向下游产品时的开发标准。

> 本文是可读性优先的集成标准；对应的**硬性契约红线**见 `24-oneui-contract-red-lines.md`（二者同源，24 为不可越线版）。

## 基本原则

OneUI 必须优先沉淀通用能力，而不是为某个业务项目写一次性控件。

当下游产品需要 UI 能力时，OneUI 的处理顺序是：

1. 判断该能力是否属于通用桌面 UI 框架能力。
2. 如果是，则在 OneUI 中设计公共 API。
3. 在 OneUI 中补控件、布局、平台能力、文档和测试。
4. 通过 OneUI 自身验证后，下游产品再接入。

## 面向 remote 的近期优先级

remote 对 OneUI 的近期要求按优先级排列：

1. Win7 兼容窗口和静态产品包。
2. 多窗口、全屏、窗口生命周期。
3. 精美但克制的桌面工具主题。
4. 表单、按钮、状态、日志等主窗口组件。
5. 实时视频 surface。
6. Raw input / RemoteInputRegion。
7. 菜单、托盘、弹窗、Toast。
8. 跨平台后端。

## API 设计约束

- 公共 API 不出现 `remote`、`device code`、`session` 等业务名词。
- 控件必须能被其他桌面工具复用。
- Win32 后端是当前真实产品后端，Linux/macOS 仍是 roadmap 时不能伪装完成。
- 每个新增能力必须能在 gallery 或示例中独立展示。
- 每个新增能力必须有文档记录状态和限制。

## 打包约束

OneUI 产品 SDK 必须坚持零终端依赖：

- 不要求用户安装 MSYS2。
- 不要求用户安装 VC Runtime。
- 不要求用户安装 UCRT。
- 不要求用户安装 Skia 或字体包。

对 remote 这类下游产品，推荐优先使用 MSVC `/MT` + vendored static Skia 的 OneUI SDK。MinGW 动态 SDK 仅能作为开发调试包，不能作为产品默认分发路径。

## 验收标准

OneUI 新增能力进入下游产品前，至少满足：

- 头文件 API 稳定且命名通用。
- 单元测试或行为测试通过。
- Gallery 或 smoke 示例可运行。
- 文档说明功能、限制、Win7 注意事项。
- 产品构建 runtime audit 通过。

## 与 remote 的协作方式

remote 的每个 UI 需求都应该反向生成 OneUI task：

```text
remote 需要能力
  -> 查 OneUI 是否已有
    -> 没有：先补 OneUI
    -> 有：remote 直接组合使用
  -> remote 集成
  -> 两边测试
```

这个流程优先于快速堆业务 UI。OneUI 要成为可开源、可复用、跨平台演进的桌面 UI 框架，就必须从第一个真实产品开始保持边界干净。
