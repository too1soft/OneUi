# Static Skia 集成

## 目的

开发模式可以使用 MSYS2 的动态 `libskia.dll` 来快速迭代，但这不是产品 SDK 目标。

产品目标：

```text
oneui.dll
  -> no libskia.dll import
  -> no HarfBuzz/FreeType/image codec DLL imports
  -> no libstdc++/libgcc DLL imports
```

## 当前模式

OneUI 有两种 Skia 模式：

```text
ONEUI_SKIA_MODE=msys2-dynamic
  快速开发模式。
  使用 C:/msys64/mingw64/lib/libskia.dll.a。

ONEUI_SKIA_MODE=bundled-static
  产品模式。
  期望 vendored static Skia 位于 third_party/skia/out/oneui-win-x64-release。
```

## 构建入口

静态 Skia 构建入口：

```powershell
.\scripts\build-skia-static.ps1 -Fetch -SyncDeps -Generate -Build
```

轻量 checkout 默认使用 shallow clone。只有明确需要完整历史时才传 `-Depth 0`。

在当前开发机上，Visual Studio 和 Windows SDK 不在 Skia 默认探测路径下，所以脚本传入：

```text
WinVc  = D:/Program Files/Microsoft Visual Studio/18/Community/VC
WinSdk = D:/Windows Kits/10
```

Windows 产品构建显式面向 Windows 7：

```text
NTDDI_VERSION=0x06010000
WINVER=0x0601
_WIN32_WINNT=0x0601
```

Skia 的 DirectWrite 后端会在部分 translation unit 周围处理这些宏，因为 Windows 头文件会按 OS 版本隐藏部分 DirectWrite interface。产品门禁以运行时导入审计为准。

## OneUI 产品构建

配置 OneUI 的 bundled static 构建：

```powershell
.\scripts\build-oneui-msvc-bundled.ps1
```

如果 Google 官方源码源较慢或不可达，可以显式传镜像和代理：

```powershell
.\scripts\build-skia-static.ps1 -Fetch `
  -DepotToolsUrl <depot-tools-git-mirror> `
  -SkiaUrl <skia-git-mirror> `
  -Proxy http://127.0.0.1:7890
```

## 审计门禁

产品模式必须通过运行时导入审计：

```powershell
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\oneui.dll -Mode product
```

Gallery 示例允许导入 `oneui.dll`：

```powershell
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe -Mode product -AllowOneUI
```

未通过审计前，SDK 只能算开发 SDK，不能算最终产品 SDK。

当前产品包路径：

```text
dist/OneUI-SDK-msvc-bundled-static
dist/OneUI-SDK-msvc-bundled-static.zip
```

当前包的目标是：`oneui.dll` 只导入 Windows 系统 DLL，Gallery 示例导入 `oneui.dll` 和系统 DLL，不复制 Skia、MSYS2 或 MSVC runtime DLL。

## 备注

从源码构建 Skia 很大且耗时，所以它与日常 OneUI 构建分离。SDK 用户和终端用户不应该执行这一步；这是发布工程任务。

仓库当前包含一个小的本地 Skia patch：`src/ports/SkImageGeneratorWIC.cpp`，用于避免 Win7-targeted 构建引用被 Windows 头文件隐藏的 WIC pixel format GUID。

