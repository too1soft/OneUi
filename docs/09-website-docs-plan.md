# OneUI 网站与文档产物

## 当前产物

`website/` 现在是一个 **Nuxt 4 + `@nuxt/ui` 应用**（不再是早期的零依赖静态站）。入口与主要文件：

- `website/nuxt.config.ts`：Nuxt 配置（`@nuxt/ui` 模块、`compatibilityVersion 4`、`nitro.prerender` 预渲染 `/`）。
- `website/app/app.vue`、`website/app/pages/index.vue`：站点外壳与文档单页。
- `website/app/data/componentReference.ts`：组件参考数据。
- `website/app/assets/css/main.css`：样式。
- `website/package.json`：依赖 `nuxt`、`@nuxt/ui`；脚本 `dev` / `build` / `generate` / `preview`。

配套的 Markdown 文档仍在 `docs/`（如 `10-form-field.md` 记录 `FormField`/`ValidationMessage`，`11-overlay-popup.md` 记录 `PopupPlacement`/`OverlayHost`），是站点内容的事实来源。

## 内容契约

网站和文档必须准确反映已接受的 runtime API。不要把尚未实现的 Linux/macOS 后端、完整 Popup 行为或完整文本系统写成已完成。

当前文档应明确：

- OneUI 是原生 C++ 桌面 UI 框架。
- Win32 后端已实现。
- Linux/macOS 仍是后端 skeleton。
- 渲染基础是 Skia raster。
- 核心模型是 `Widget`、`View`、`Canvas`、`OverlayHost`。
- 样式方向是 typed CSS-like style，而不是运行时 CSS parser。
- 作者体验方向是 HTML/Vue3/MVVM。

## 已接受 API 摘要

`FormField`：

- `setChild`
- `setLabel` / `bindLabel`
- `setHelperText` / `bindHelperText`
- `setErrorText` / `bindErrorText`
- `setRequired` / `bindRequired`
- `setInvalid` / `bindInvalid`
- `setStyleOverride`
- `clearStyleOverride`

`ValidationMessage`：

- `setText` / `bindText`
- `setTone`
- `setStyleOverride`
- `clearStyleOverride`

行为规则：

- `invalid == true` 时，error text 优先于 helper text。
- `invalid == false` 时，有 helper text 就显示 helper text。
- Gallery 的 `Controls` 区域用于检查 FormField 示例。
- Gallery 的 `Style` 区域用于检查主题、状态和 override 行为。

## 作者体验方向

OneUI 应让 HTML、Vue3 和 MVVM 开发者感觉自然，同时保持原生 C++：

- 组合是显式的，例如 `FormField::setChild(...)`。
- 响应式状态使用强类型 `State<T>` binding API。
- 样式自定义使用 typed override struct 和 theme token。
- 布局使用 `Stack`、`Grid`、`ScrollView` 等控件树容器。

## 本地运行与检查

在 `website/` 下：

```text
npm install
npm run dev        # 本地开发，nuxt dev --host 127.0.0.1
npm run generate   # 产出静态站点用于发布
```

建议手动检查：

- 导航到 FormField，确认 API 与行为说明存在。
- 确认 `invalid == true` 的错误文本优先规则写清楚。
- 确认 Gallery 说明包含 `.\scripts\run-gallery.ps1`。
- 检查文案没有 mojibake 或损坏双语。

## SDK 打包

`scripts/package-sdk.ps1` 会复制 `docs/*.md`，并在 `website/` 存在时把它纳入 SDK 的 `website/` 目录。

