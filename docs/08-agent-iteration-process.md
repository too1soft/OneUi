# OneUI 多 Agent 迭代流程

本文档固定 OneUI 的持续迭代方式。每一轮都必须以可验证的产品结果结束；如果 Tester 没有接受，工作不能被标记为完成。

## 关键校正

OneUI 是原生 C++ 桌面 UI 仓库，不是 Web package workspace。任务描述应使用真实路径：

```text
include/oneui/**
src/core/**
src/platform/**
examples/gallery/**
tests/**
docs/**
website/**
scripts/**
cmake/**
CMakeLists.txt
```

不要分配 `packages/oneui/**`、`apps/gallery/**` 这类不存在路径。运行时工作应使用 OneUI 术语：widget tree、`View`、`Widget`、`Canvas`、`OverlayHost`、paint order、hit-test order、layer token、Gallery、SDK package。

当前产品策略是 breadth-first：先快速补齐 UI library surface，再回头打磨细节。优先完整可用的骨架、Gallery 覆盖、文档和作者示例；只有阻塞正常使用的 bug 才优先深挖。

## 五 Agent 连续任务池

五个 Developer agent 应处理五个相互独立的任务，而不是默认把同一任务拆成五份。只有用户明确要求拆分同一任务时才这样做。

流程：

1. PM 从 backlog 中选择独立任务并定义验收标准。
2. UI agent 在需要时细化交互、状态和视觉要求。
3. Developer 1-5 分别处理 disjoint scope。
4. 任一 Developer 完成后，结果直接进入 Tester。
5. Tester 接受后，Website Docs Developer 更新 docs/site。
6. Tester 再验证 docs/site artifact。
7. 被释放的 Developer 立即领取下一项 PM 任务。

如果写入范围重叠，PM 必须串行化或换任务。不能通过并行制造冲突。

## 永久产品规则

PM 每轮都必须 enforce：

- OneUI 是面向 Windows、Linux、macOS 的原生 C++ 桌面 UI 框架；当前实现重点是 Win32。
- Windows 7 兼容性和零终端用户安装依赖是产品约束。
- 作者体验应靠近 HTML/Vue3/MVVM，但运行时仍是原生 C++。
- 复用视觉概念必须尽量抽象为 type、token 或 style object。
- 样式模型保持 CSS-like freedom：theme token、状态样式、组件 override、布局 props。
- Gallery、docs、website 应展示真实工作流，不只是孤立控件。
- 每个公开组件都应可从 SDK 导出，必要时覆盖 Gallery，并反映到组件清单。
- 文档应中文优先；公开网站可以提供中英切换，但不能留下损坏双语或 mojibake。

## 角色

### Product Manager Agent

负责产品范围、顺序和验收标准。

输出：

- 任务标题。
- 用户问题。
- 范围。
- 非目标。
- UI 要求。
- style/theme 抽象要求。
- 文档/Gallery/网站要求。
- 验收标准。
- 并行拆分和文件 ownership。

### UI Agent

负责把 PM 任务细化为原生桌面 UI 的具体体验。

输出：

- 视觉结构与布局。
- 交互状态。
- style/token 要求。
- Gallery/website demo 说明。
- 需要测试的风险。

### Developer Agents

负责实现分配给自己的 disjoint scope。

要求：

- 只实现 PM/UI 批准的范围。
- 尊重其他 agent 的文件 ownership。
- 使用现有 OneUI 模式和可复用抽象。
- 添加或更新相关测试。
- 不破坏 SDK 打包承诺。

### Website Docs Developer Agent

负责 Tester 接受后的 docs/site 更新。

要求：

- 只改分配的 docs/site 范围，除非明确要求 runtime 改动。
- 文档中文优先，必要时保留英文术语。
- 记录组件 API、MVVM 用法、style/theme 自定义和 Gallery 使用方式。
- 如果 website 尚不完整，至少补齐可迁移的 docs/site artifact。

### Tester Agent

负责质量门禁。

检查：

- 实现是否符合 PM 验收标准和 UI notes。
- 是否绕过 style/theme 抽象。
- Gallery、docs、website 是否满足本轮要求。
- 构建、测试、运行时导入审计、SDK 包和 SDK consumer 是否通过。
- Gallery 是否从产品 SDK 启动。

决策：

- `ACCEPT`：所有门禁通过，且 docs/site artifact 已验证。
- `REJECT`：列出具体 bug，退回对应 developer。
- `PROCESS_EXCEPTION`：关键角色缺失、工具失败、范围冲突或用户中止。

## 必需循环

```text
PM defines product slice
  -> UI refines interaction and style requirements
    -> Developers implement disjoint scopes
      -> Tester verifies runtime/product result
        -> if rejected: return to owner and retest
        -> if accepted: Website Docs Developer updates docs/site
          -> Tester verifies docs/site artifact
            -> PM selects next slice
```

## 证据包

交给非开发角色前，应提供紧凑证据包：

- 目标 slice 与非目标。
- 已改或可能改的文件。
- 公共 API 名称。
- 构建、测试、审计、打包结果。
- Gallery 截图或视觉说明。
- 当前 open issues 和需要该角色做出的决定。

## 必需验证链

常规产品验证：

```powershell
C:\msys64\mingw64\bin\cmake.exe --build --preset mingw64
C:\msys64\mingw64\bin\ctest.exe --test-dir build\mingw64 --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-oneui-msvc-bundled.ps1
ctest --test-dir build\msvc-bundled-static --output-on-failure
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\oneui.dll -Mode product -Toolchain mingw64
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe -Mode product -Toolchain mingw64 -AllowOneUI
.\scripts\package-sdk.ps1 -Toolchain msvc-bundled-static
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-sdk-consumer.ps1
```

文档-only 任务不需要跑完整产品构建，但必须至少做目标文档范围扫描和 Markdown 可读性检查。

## 当前推荐 breadth-first slices

1. CSS-like style system：状态样式、theme token、component override。
2. Gallery/docs/site i18n：中文优先文档、公开网站中英切换、无 mojibake。
3. Popup/OverlayHost：Select、Menu、Tooltip、Dialog、Toast 共用基础。
4. ScrollView：wheel、scrollbar、keyboard、horizontal direction。
5. FormField 和 validation message。
6. IconButton、SegmentedControl、Toolbar。
7. Dialog、Tooltip、Toast、Menu MVP。
8. 产品网站/docs site skeleton。
9. 文本测量和 text layout。
10. TextField selection、clipboard、IME planning。

