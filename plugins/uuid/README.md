# OneUI UUID

独立的 UUIDv4 原生服务插件，使用系统随机数服务；不需要 OneUI 渲染器。公开服务为 `oneui.uuid.generator`，Plugin ABI 1。Windows 使用 BCrypt，macOS 源码后端使用 Security.framework。

从官网下载的 Plugin SDK 构建插件：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/native-sdk -DCMAKE_INSTALL_PREFIX=/path/to/uuid
cmake --build build
cmake --install build
```

Windows 使用匹配 SDK 的 MSVC x64 开发环境和静态 CRT。通过 `oneui-site plugin add --id oneui.uuid --version ^0.1.0` 安装时，可由项目锁管理该 SDK 依赖。

完整命令、CMake、C API 示例和错误规则见 [UUID 插件接入指南](../../docs/plugin-uuid.md)。
