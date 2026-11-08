# 预编译 SDK 平台矩阵

按操作系统、架构、链接方式和 ABI 选择 SDK。最新公开核心 SDK 为 `0.1.0-preview.4`（ABI 46，Windows modern x86_64）；其他目标提供下表所列的历史版本。全部包通过预览渠道分发。

| 目标 | 产物 | 系统与工具链 | 公开下载版本 |
|---|---|---|---|
| Windows modern x86 | DLL / LIB | MSVC 19.50，静态 CRT；Windows 10/11 | preview.1 / ABI 43 |
| Windows modern x86_64 | DLL / LIB | MSVC 19.50，静态 CRT；Windows 10/11 | preview.4 / ABI 46 |
| Windows legacy x86 | DLL / LIB | MSVC 19.44，静态 CRT；Win7 SP1 起 | preview.1 / ABI 43 |
| Windows legacy x86_64 | DLL / LIB | MSVC 19.44，静态 CRT；Win7 SP1 起 | preview.1 / ABI 43 |
| macOS arm64 | dylib / a | 部署目标 macOS 13；Apple Clang / libc++ | preview.1 / ABI 43 |
| Linux x86_64 | so / a | Ubuntu 24.04；Clang 18 / libstdc++；glibc 至少 2.38 | preview.1 / ABI 43 |

Linux 包适用于所列 Ubuntu/glibc 环境。macOS Intel、Linux ARM64 尚无上述核心 SDK 预编译下载项。各版本头文件、Rust 绑定和运行库必须成套使用。

无渲染依赖的原生服务使用独立 `oneui.native-sdk` 包和 Plugin ABI 1，与核心 SDK 的平台矩阵分别管理。服务插件的目标产物见[插件目录](https://oneui.too1.net/plugins)；UI 组件包另需满足核心 SDK ABI 要求。

`oneui.native-sdk 0.1.1` 和 [UUID 插件](plugin-uuid.md)提供 Windows modern x86_64 与 macOS native arm64 的静态/动态 SDK 配置。macOS 部署目标为 13.0，使用 Apple Clang 17 / libc++，不需要核心 UI SDK 或 Skia。

## Linux 系统依赖

OneUI 与随附 Skia 库已经编译，但 GTK、GLib、X11、Wayland、图形驱动与系统字体仍由目标系统提供。Ubuntu 24.04 动态运行所需的直接依赖包括：

```sh
sudo apt install libgtk-3-0t64 libayatana-appindicator3-1 \
  libegl1 libgles2 libwayland-client0 libwayland-cursor0 libwayland-egl1 \
  libx11-6 libxrandr2 libxkbcommon0 libfontconfig1 libfreetype6
```

静态 SDK 的消费者还需要链接入口和 pkg-config 元数据：

```sh
sudo apt install build-essential cmake pkg-config libgtk-3-dev \
  libayatana-appindicator3-dev libegl1-mesa-dev libgles2-mesa-dev \
  libwayland-dev libx11-dev libxrandr-dev libxkbcommon-dev \
  libfontconfig1-dev libfreetype-dev
```

这些系统包不是重编 OneUI。发行版包名可能变化，应以目标系统的软件源为准。Wayland 的定位、主动激活、托盘以及输入法支持仍取决于桌面协议和环境；调用能力查询接口后处理不支持结果。

## Windows 分发

动态 SDK 中的 oneui.dll 与应用一起分发。legacy 与 modern 不混用；C++ 静态消费者使用匹配工具链和静态 CRT。Win7 缺少的系统能力由 SDK 返回不支持或服务不可用，不将通知接受请求伪装成通知已经显示。

当前 SDK DLL 未进行正式 Authenticode 签名。发布业务安装包时应按产品的发布流程签名并重新验收最终产物。

## macOS 分发

动态库采用 @rpath 身份，业务应用通常将它放入 .app/Contents/Frameworks 并设置 @executable_path/../Frameworks。发布签名和公证由最终应用发布流程完成；本预览包不声称已通过 Developer ID 或公证。

静态包仍会链接 Cocoa、Metal、UserNotifications 等系统框架。通知冷启动需要正确的应用包身份及权限流程，不能仅靠一个裸命令行消费者替代完整应用验收。

## 移动应用

Android、iOS 与 HarmonyOS 的宿主接口及接入方式见[移动端支持](mobile-support.md)。下载中心当前提供上述桌面目标；移动宿主按对应的平台工程配置接入。
