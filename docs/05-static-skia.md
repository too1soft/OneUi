# Static Skia 集成

## 目的

开发模式保留 MSYS2 动态 `libskia.dll` 配置，但必须提供与固定修订匹配的完整文字依赖。
现有 milestone 143 安装不满足 milestone 150 的 SkParagraph 构建要求；当前 MinGW
文字依赖仍待匹配重建，不能把旧开发包的通过记录沿用到本轮。这也不是产品 SDK 目标。

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

默认 Windows 产品路线为 `win10`，继续使用当前工具链。Win7 SP1 使用独立的
`-MinimumWindows win7` 配置，固定 MSVC 14.44.35207、SDK 10.0.22621.0、静态 CRT，
并为 x86/x64 分别重建完整 Skia 依赖。默认输出增加 `-win7` 后缀，不替换现代 SDK。
只有 Win7 配置使用下列 API 宏：

```text
NTDDI_VERSION=0x06010000
WINVER=0x0601
_WIN32_WINNT=0x0601
```

API 宏并不能决定链接进来的 CRT 是否支持 Win7。Win7 发布必须同时完成匹配架构的
原版系统导入审计与真实运行验收；现代 MSVC 14.50 CRT 含 Win8 专用导入，构建会拒绝
将其标成 Win7。完整配方和验收边界见[Win7 兼容说明](24-windows7-compatibility.md)。

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

### 共享 Windows 兼容基础库

PE 解析和审计入口由独立 `windows-compat` 项目提供，本仓库通过
`scripts/windows_compat_loader.py` 加载。默认目录为仓库同级 `../windows-compat`，
也可设置 `WINDOWS_COMPAT_ROOT` 指向已有 checkout。运行前会校验 `compat.lock.json`
列出的所有基础库文件；缺失或哈希不匹配会失败，不自动使用另一版本。
该基础库不包含在本仓库中，首次运行 Python 审计、相关测试或 Win7 打包前必须准备匹配内容。

OneUI 本身的现代 C++ 构建与加载共享 Python 审计库是不同步骤。
完整 SDK 的 `sdk-inputs-<Configuration>.json` 由 CMake 生成，双动态/静态 SDK 打包工具
位于共享基础库 `scripts/package-oneui-sdk.py`。静态 Rust 消费者使用
`ONEUI_SDK_DIR` 和 `ONEUI_SDK_MANIFEST_SHA256` 校验清单；Win7 不回退到手写链接库列表。

产品模式必须通过运行时导入审计：

```powershell
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\oneui.dll -Mode product
```

Gallery 示例允许导入 `oneui.dll`：

```powershell
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe -Mode product -AllowOneUI
```

上述默认审计仅检查产品依赖，不证明 Win7 支持。Win7 另需提供从合法原版 SP1 系统
提取的同架构导出元数据（不分发系统 DLL）：

```powershell
.\scripts\audit-runtime.ps1 -Binary D:\oneui-win7\oneui.dll -Mode product `
  -MinimumWindows win7 -WindowsReference D:\references\win7-sp1-x86.json
```

导入审计通过仍不等于原生 UI 验收完成。没有匹配架构参考或运行环境时保留待验状态。

当前产品包路径：

```text
dist/OneUI-SDK-msvc-bundled-static
dist/OneUI-SDK-msvc-bundled-static.zip
```

当前包的目标是：`oneui.dll` 只导入 Windows 系统 DLL，Gallery 示例导入 `oneui.dll` 和系统 DLL，不复制 Skia、MSYS2 或 MSVC runtime DLL。

## 备注

从源码构建 Skia 很大且耗时，所以它与日常 OneUI 构建分离。SDK 用户和终端用户不应该执行这一步；这是发布工程任务。

依赖补丁不止早期的 WIC GUID 修正：还包括固定 ICU 的 Bidi 修复及 Win7 DirectWrite /
字体回退补丁。配方、哈希校验与适用范围见[补丁维护说明](../third_party/patches/README.md)。
“静态 Skia”指链接方式，不代表只能软件渲染；Win32 GPU 路径见[渲染说明](39-rendering-and-validation.md)。
