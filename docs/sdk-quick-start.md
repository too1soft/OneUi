# 使用预编译 SDK

业务项目直接使用下载中心的 SDK。无需克隆 OneUI、编译 Skia，或在业务项目里调用 OneUI 的源码构建脚本。第一次接入选择预览版中与你的系统、架构和工具链匹配的包。

## 选择下载包

- **动态 SDK**：包含运行库、匹配头文件、导入库（Windows）、CMake 文件、C/C++ 示例和 Rust 绑定。适合常规接入。
- **仅 DLL**：Windows 动态版可单独下载运行库。下载文件保留版本与架构名称；分发应用时重命名为 `oneui.dll`，放到程序旁。它不包含头文件或导入库，首次接入仍应下载完整 SDK，不可混用不同版本或架构。
- **静态 SDK**：包含 OneUI 及随附第三方静态库，CMake 负责链接关系。目标系统的原生桌面库仍是系统依赖。
- **Win7**：必须选择 legacy 兼容包。modern 包不承诺兼容 Win7。
- Mac 当前提供 arm64；Linux 当前提供 Ubuntu 24.04 x86_64。其他目标见[支持矩阵](sdk-platforms.md)。

解压后目录应直接包含 `sdk-manifest.json`、`include`、`lib`、`cmake` 和 `examples`。Windows 动态库在 `bin` 中。

## C++ / C：先跑 SDK 内的最小示例

准备你自己的 C/C++ 编译器和 CMake。Windows 使用 Visual Studio 的开发者终端，架构应与包一致；例如 x64 开发者终端配 x86_64 SDK。

```sh
cmake -S /absolute/path/to/sdk/examples -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/sdk
cmake --build build --config Release
```

Windows 的 CMake 示例会把动态库复制到程序旁。Linux/macOS 示例设置指向 SDK 的构建期 rpath；正式应用应将运行库随安装包携带，并设置可移动的运行时路径。

示例程序创建窗口后自动关闭，用于确认接入和基本事件循环。它不会构建 OneUI。

## 接入自己的 CMake 项目

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(OneUI CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE OneUI::oneui)
if(WIN32 AND ONEUI_SDK_LINKAGE STREQUAL "shared")
  add_custom_command(TARGET my_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:OneUI::oneui> $<TARGET_FILE_DIR:my_app>)
endif()
```

OneUI::oneui 由所下载的包决定是动态还是静态。静态包使用已经编译好的库；构建时只编译业务代码并链接。Windows 包使用静态 CRT，CMake 接入文件会对 MSVC 消费者设置对应运行库。

## 使用共享缓存与项目锁

交付的 Go 程序同时提供 SDK 下载命令。将它改名为 `oneui-site`（Windows 为 `oneui-site.exe`）并加入 PATH。正式域名为 `oneui.too1.net`；以下命令需在官网实际部署后使用，下载页会按当前站点生成完整命令。

```sh
oneui-site sdk fetch --url https://oneui.too1.net \
  --version 0.1.0-preview.1 \
  --target windows-modern-x86_64 --linkage shared
```

命令输出本机 SDK 的绝对目录，同时在当前项目写入 `oneui.lock.json`。把锁文件提交到业务仓库；缓存目录不提交。将输出目录传给 CMAKE_PREFIX_PATH 即可。

同一台机器不同项目会复用相同 SHA256 的缓存。每个锁条目包含版本、目标、包的 SHA256 和内部清单哈希；再次使用时校验 SDK 文件，没有隐式源码构建。

```sh
# 在同一项目中离线恢复；仍会校验实际缓存文件
oneui-site sdk fetch --target windows-modern-x86_64 --offline

# 升级时明确指定新版本并使用 --update
oneui-site sdk fetch --url https://oneui.too1.net \
  --version YOUR_NEW_VERSION --target windows-modern-x86_64 --update
```

本地 HTTP 预览服务需要显式加 `--allow-http`；公开部署应通过 HTTPS 反向代理提供服务。下载请求不接受跨域重定向。SHA256 用于内容一致性，不替代发布者签名。

## Rust 接入

Rust 使用包内匹配版本的 `bindings/rust`，设置 ONEUI_SDK_DIR。详细的 Cargo 示例和动态库分发见 [Rust 接入](sdk-rust.md)。

## 运行库放在哪里

开发阶段 SDK 可以共享缓存，最终应用安装包应携带自己锁定版本的运行库。不要依靠一份可以被其他应用覆盖的全局 DLL。SDK 的头文件、静态库、开发文档不需要全部发给最终用户。

## 从源码开发 OneUI

本工作台由专门负责 OneUI 的会话维护源码、平台适配和正式构建；其他项目需要构建 OneUI 时须先取得用户明确批准。业务项目统一使用本页的预编译接入。官网缺包时报告给维护者，不自动回退到源码构建。获准的维护工作使用[源码构建指南](12-getting-started.md)。
