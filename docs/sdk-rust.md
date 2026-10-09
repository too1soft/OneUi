# Rust 使用预编译 SDK

SDK 的 bindings/rust 目录随版本包含 oneui 和 oneui-sys。它们是包装层，链接已经编译好的原生库；不会自动克隆或编译 OneUI C++ 源码。

## Cargo 项目

将下面路径替换为实际解压或缓存目录。SDK 中的 Rust workspace 已独立组织，只包含这两个包。

```toml
[dependencies]
oneui = { path = "/absolute/path/to/sdk/bindings/rust/oneui" }
```

编译时设置 SDK 目录：

```sh
export ONEUI_SDK_DIR=/absolute/path/to/sdk
cargo build --release
```

Windows PowerShell：

```powershell
$env:ONEUI_SDK_DIR = 'C:/path/to/sdk'
cargo build --release
```

Cargo 仍需编译业务 Rust 代码和包装层，这与重新编译 OneUI、Skia 不同。Rust 自身依赖按 Cargo.lock 管理。

## 静态链接

下载静态 SDK，并打开 static-link：

```toml
[dependencies]
oneui = { path = "/absolute/path/to/sdk/bindings/rust/oneui", features = ["static-link"] }
```

Windows 静态包使用静态 CRT。消费者的 .cargo/config.toml 按目标设置：

```toml
[target.x86_64-pc-windows-msvc]
rustflags = ["-C", "target-feature=+crt-static"]
```

x86 和 Win7 特定 Rust target 应使用对应包和经过产品验证的工具链。将 oneui.lock.json 中的 manifestSha256 通过 ONEUI_SDK_MANIFEST_SHA256 传入，可检查项目锁定清单。包内静态库与系统链接信息来自实际构建闭包。

## 动态运行库

Windows 把 SDK 的 bin/oneui.dll 放到应用 EXE 旁。Linux/macOS 应用为 liboneui.so / liboneui.dylib 设置正确的运行时查找路径，并在安装包中携带对应库。构建脚本的测试 rpath 不替代最终打包配置。

## API 与生命周期

使用 safe oneui 包处理窗口和控件生命周期；需要尚未包装的能力时，可查看 oneui-sys 和 UTF-8 C ABI。图形窗口与跨窗口控件迁移运行在 UI 线程。不要从后台线程直接操作原生控件；使用 dispatcher 交付结果。
