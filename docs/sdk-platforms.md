# 预编译 SDK 平台矩阵

这里描述当前 0.1.0-preview.1 下载包。平台实现、自动化测试、真实系统验收和发布签名分别记录。全部包目前处于预览渠道。

| 目标 | 产物 | 系统与工具链 | 已有证据 / 限制 |
|---|---|---|---|
| Windows modern x86 | DLL / LIB | MSVC 19.50，静态 CRT；Windows 10/11 目标 | Windows 10 x64 主机运行 x86 消费者；不代替独立 x86 系统验证 |
| Windows modern x86_64 | DLL / LIB | MSVC 19.50，静态 CRT；Windows 10/11 目标 | Windows 10 x64 SDK/消费者回归；Windows 11 完整原生验收待完成 |
| Windows legacy x86 | DLL / LIB | MSVC 19.44，静态 CRT；Win7 SP1 起 | Win7 SP1 x86 虚拟机已运行定向 SDK 与消费者测试；不代表全部业务和硬件验收 |
| Windows legacy x86_64 | DLL / LIB | MSVC 19.44，静态 CRT；Win7 SP1 起 | 编译和 Windows 10 主机回归；Win7 x64 原生验收待完成 |
| macOS arm64 | dylib / a | 部署目标 macOS 13；Apple Clang / libc++ | 物理 Mac 构建与消费者验证；通知等历史原生交互证据不等于本下载包全面认证 |
| Linux x86_64 | so / a | Ubuntu 24.04；Clang 18 / libstdc++；glibc 至少 2.38 | WSLg / Xvfb 自动回归；真实 X11/Wayland 桌面、输入法、跨屏 DPI 待验收 |

Linux 当前按 Ubuntu 24.04 构建，不能仅凭架构相同就宣称兼容旧 glibc、麒麟、UOS 或其他发行版。macOS Intel、Linux ARM64 暂无此版本预编译包。

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

## 移动端开发状态

Android、iOS 和原生 HarmonyOS 已开始开发，**目前没有移动 UI SDK 下载包**。
公共运行时与原生宿主的阶段、接口和未完成项见 [移动端支持](mobile-support.md)。
上述桌面发行目标和现有预编译包保持各自的验收范围。
