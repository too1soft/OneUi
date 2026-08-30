# OneUI 文档索引

本目录同时包含三类资料：

1. **当前使用文档**：描述当前代码、构建、组件和公开契约；
2. **强制规范**：定义框架/产品边界、平台红线、输入和 ABI 规则；
3. **设计与演进记录**：保存某个阶段的方案、差距分析和复盘，不一定代表当前 API。

查找“现在支持什么”时，优先阅读 README、组件清单、组件参考和公开头文件，不要把早期
roadmap 或差距分析当成现状。

## 推荐阅读顺序

| 顺序 | 文档 | 用途 |
| --- | --- | --- |
| 1 | [项目 README](../README.md) | 项目定位、能力摘要、构建和限制 |
| 2 | [入门指南](12-getting-started.md) | 从源码构建、运行 Gallery、接入 SDK、运行测试 |
| 3 | [组件清单](07-component-inventory.md) | 当前组件、成熟度、C ABI/Rust 覆盖与限制 |
| 4 | [组件参考](14-component-reference.md) | 分组 API、交互契约和使用建议 |
| 5 | [架构](01-architecture.md) | 模块边界、线程、所有权、渲染与互操作层次 |
| 6 | [样式表](20-style-sheet.md) | CSS-like 选择器、属性、状态和 typed adapter |
| 7 | [C ABI 接入](c-abi-integration.md) | UTF-8 ABI v16、句柄、数组、回调、线程与版本检查 |
| 8 | [Rust 绑定](../bindings/rust/README.md) | `oneui-sys`、安全层、dispatcher、handle 和回调生命周期 |

## 当前规范

### 架构、平台与质量

- [愿景](00-vision.md)
- [架构](01-architecture.md)
- [依赖策略](04-dependency-policy.md)
- [静态 Skia](05-static-skia.md)
- [设计语言](06-design-language.md)
- [组件开发指南](13-authoring-guide.md)
- [可访问性](15-accessibility.md)
- [平台后端契约](28-platform-backend-contract.md)
- [DPI 与 GDI 缩放契约](29-dpi-and-gdi-scaling-contract.md)

### 产品接入与红线

- [下游产品接入规范](17-downstream-product-integration-standard.md)
- [AppShell](18-app-shell.md)
- [ProductShell](19-product-shell.md)
- [OneUI-first 原则](23-oneui-first-pivot.md)
- [OneUI 契约红线](24-oneui-contract-red-lines.md)
- [UI 红线：禁止产品 magic special case](26-ui-red-lines-no-magic-special-cases.md)
- [运行时输入红线](31-runtime-input-red-lines.md)

### 组件专题

- [FormField](10-form-field.md)
- [Overlay / Popup](11-overlay-popup.md)
- [VirtualList](17-virtual-list.md)
- [重排契约](25-reorder-contract.md)
- [TerminalView](33-terminal-view.md)
- [TreeView](34-tree-view.md)
- [Win32 滚轮复盘](35-win32-wheel-scrolling-retrospective.md)

### C ABI 与远程/产品能力

- [C ABI 接入](c-abi-integration.md)
- [UTF-8 CSS/ABI 实现说明](25-remote-oneui-css-abi-implementation.md)
- [远程渲染性能案例](27-performance-case-remote-rendering.md)
- [Overlay fill/input E2E 案例](30-overlay-fill-and-input-e2e-case.md)
- [iShellPro 原生就绪性](32-ishellpro-native-readiness.md)

## 设计与演进记录

以下文档用于解释历史决策、规划或当时的缺口。实现可能已经超过其中描述：

- [路线图](02-roadmap.md)
- [样式设计初稿](03-style.md)
- [Agent 迭代流程](08-agent-iteration-process.md)
- [网站文档计划](09-website-docs-plan.md)
- [远程客户端差距分析](16-remote-client-requirements-gap.md)
- [Material 3 控件标准](22-material3-control-standard.md)

## 权威来源优先级

出现冲突时按以下顺序判断：

1. `include/oneui/oneui_c_api.h`：C ABI 版本与函数签名；
2. `include/oneui/**/*.h`：公开 C++ API；
3. `bindings/rust/oneui-sys/src/lib.rs`：Rust FFI 映射；
4. `bindings/rust/oneui/src/lib.rs`：安全 Rust API 与生命周期；
5. `tests/`：已被自动验证的行为契约；
6. 本目录中的当前使用文档；
7. 设计与演进记录。

## 文档维护规则

- 新增公开组件时同步更新 `07-component-inventory.md` 和 `14-component-reference.md`；
- 修改 C ABI 时同步更新 ABI 版本、`oneui-sys`、安全绑定、测试和 C ABI 文档；
- 修改构建/打包路径时同步更新中英文 README 与 `12-getting-started.md`；
- 已被实现反转的历史文档不要静默当成现状，应在当前文档中明确覆盖关系；
- 示例命令必须在仓库根目录可执行，Windows 路径优先使用 PowerShell；
- 对“完整支持”“生产级”等表述必须给出测试或限制依据。
