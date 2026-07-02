# 依赖策略

## 术语

OneUI 有两类使用者：

- **SDK 用户**：安装 OneUI SDK 并用它构建应用的开发者。
- **终端用户**：安装并运行 OneUI 应用的人。

依赖策略主要保护终端用户，同时也尽量避免 SDK 用户手工配置第三方库。

## 目标

用 OneUI 构建的应用不应要求终端用户安装额外运行时依赖。

对终端用户意味着：

- 不要求安装 MSYS2。
- 不要求安装 Visual C++ Runtime。
- 不要求 Windows 7 用户额外安装 Universal CRT。
- 不要求单独安装 Skia DLL。
- 不要求单独安装字体包。

操作系统自带 API 可以使用。需要客户另行安装的 redistributable runtime package 不应成为产品依赖。

## SDK 用户要求

SDK 用户预期只需要：

- 支持的编译器和工具链。
- CMake 或文档化的集成方式。
- OneUI SDK。

SDK 用户不应额外安装或配置 Skia、HarfBuzz、FreeType、图片 codec 或 fallback font。

## 开发依赖与产品依赖

MSYS2 包可以作为临时开发 bootstrap，但不是产品打包策略。

产品策略：

- 第三方源码放入 `third_party/`。
- 许可证允许时构建为静态库。
- OneUI demo/app 产物应自包含，或者放入自包含应用目录。
- 系统字体不可控时，把必要 fallback font 作为 OneUI asset 打包。

## 渲染与文本

Skia 是形状和文本绘制的渲染基础。

普通文本绘制不应在常规路径中走 GDI `DrawText` 这类平台绘制 API。平台字体 API 可以用于发现系统字体，但 glyph 测量、rasterization 和绘制应属于 renderer/text 子系统。

目标文本栈：

```text
OneUI Text
  -> HarfBuzz shaping
  -> Skia font and glyph rendering
  -> Bundled fallback fonts
```

## Windows 7 产品规则

Windows 7 是一等目标。Windows 产品构建必须避免要求终端用户安装 UCRT 或 Visual C++ Runtime。

当前 Windows 产品工具链方向：

```text
MSVC release build
  -> /MT static MSVC runtime
  -> vendored static Skia
  -> oneui.dll + oneui.lib SDK shape
  -> no Skia/MSYS2/VC runtime installer for end users
```

MSYS2 dynamic package 仅适合本地开发 demo。

