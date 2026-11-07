# 安装和使用插件包

OneUI 插件让应用按需接入原生服务和扩展组件。应用选择包与版本，工具解析依赖、下载匹配目标的产物，并生成 CMake 接入文件。原生服务插件可以独立于 UI 绘制引擎使用。

## 安装 VPN 插件

准备 `oneui-site` 命令、CMake、Ninja 和目标包对应的编译器。在应用项目目录执行：

```sh
oneui-site plugin add --id oneui.vpn --version "^0.1.0" \
  --target windows-modern-x86_64 --linkage shared \
  --url https://oneui.too1.net
```

命令解析并锁定 `oneui.vpn` 与它依赖的原生 SDK，输出生成的 CMake 文件路径。将 `oneui.plugins.json` 和 `oneui.lock.json` 提交到应用仓库；`.oneui` 目录和共享缓存无需提交。

静态与动态包分别锁定。选择包时匹配系统、架构、工具链和 C++ 运行库。安装命令不编译框架或 Skia。

## 接入 CMake

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES C CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/.oneui/plugins/windows-modern-x86_64-shared/OneUIProjectPlugins.cmake")
add_executable(my_app main.c)
oneui_target_plugins(my_app REGISTRAR register_app_plugins
    PLUGINS ${ONEUI_PLUGIN_TARGETS})
set_target_properties(my_app PROPERTIES LINKER_LANGUAGE CXX)
```

应用包含生成的 `oneui_generated_plugins.h`，创建 `OneUiPluginRuntime`，调用 `register_app_plugins(runtime)` 后启动运行时。Windows 构建会把插件依赖的运行库复制到程序目录。应用分发时随安装包携带这些运行库。

通过 `oneui_plugin_runtime_acquire_v1` 取得服务租约，再读取版本化 C 函数表。使用 VPN 时检查 `OneUiVpnPluginApiV1` 的大小与 ABI，创建会话并传入目标平台宿主。完整示例见 [VPN 插件消费者](../examples/plugins/vpn_app/README.md)。

## 生命周期

每个应用或引擎实例拥有自己的插件运行时。管理调用在创建线程完成；后台工作按服务接口的线程约定执行。启动会先检查完整依赖图，按依赖顺序初始化，关闭按反序清理。

服务租约保持实例和函数表有效。销毁会话、取消订阅并停止服务调用后再释放租约。随后停止运行时；原生资源仍在清理时返回 `BUSY`，继续处理平台回调后重试，成功后再销毁运行时。

VPN 的授权、系统隧道和包引擎沿用 [VPN SDK](vpn-sdk.md) 契约。插件权限声明不代替系统授权。

## 恢复、更新与移除

```sh
# 依照项目锁恢复，不改变版本
oneui-site plugin restore --target windows-modern-x86_64 --linkage shared

# 断网时从已验证的本地缓存恢复
oneui-site plugin restore --target windows-modern-x86_64 --linkage shared --offline

# 按声明的版本范围重新解析
oneui-site plugin restore --target windows-modern-x86_64 --linkage shared --update

# 移除直接依赖及不再需要的间接依赖
oneui-site plugin remove --id oneui.vpn --target windows-modern-x86_64 --linkage shared

oneui-site plugin list
```

缓存文件校验失败时，命令停止并指出问题；确认没有项目正在使用后，可移除对应损坏缓存项再恢复。移除项目依赖不会删除共享缓存。修改声明、移动应用目录或更换缓存位置后，重新执行恢复命令。

## 本地与 Git 源码包

```sh
oneui-site plugin add --id example.service --version "^1.0.0" \
  --path ../example-service --target windows-modern-x86_64 --linkage shared

oneui-site plugin add --id example.service --version "^1.0.0" \
  --git https://example.com/team/example-service.git --revision FULL_40_CHARACTER_COMMIT \
  --target windows-modern-x86_64 --linkage shared
```

源码包根目录包含 `oneui-plugin.json` 和 CMake 工程。Git 包锁定完整提交，URL 不包含凭据；私有仓库使用本机 Git 凭据助手。编译应用时构建所选插件自身，基础 SDK 仍使用预编译包。本地包元数据改变后，用 `restore --update` 明确更新项目锁。

## 包来源与兼容性

应用可通过 `--url` 使用自己的 HTTPS 插件目录。下载按目录和项目锁校验归档及每个文件的 SHA256；版本冲突、循环依赖、重复提供者和目标缺包都有明确诊断。

插件是随应用构建的原生代码，使用应用进程的权限。选择可信的包来源，并随应用版本一起管理插件升级。
