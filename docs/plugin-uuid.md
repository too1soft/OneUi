# 使用 UUID 原生服务插件

`oneui.uuid` 为应用生成标准小写 UUIDv4，可用于本地记录 ID、任务 ID 和请求关联 ID。它使用系统随机数服务，不需要 OneUI 窗口、渲染器或 Skia。

Windows 后端调用 BCrypt；macOS 源码后端调用 Security.framework。当前预编译下载提供 Windows 10/11 x64，使用 MSVC 19.50 与静态 CRT。下载项的 `static/shared` 表示所消费的 Plugin SDK 链接方式；UUID 插件本身以静态库随应用链接。

## 安装到应用

在现有应用目录执行：

```powershell
oneui-site plugin add --id oneui.uuid --version ^0.1.0 `
  --project . --target windows-modern-x86_64 --linkage shared `
  --url https://oneui.too1.net
```

工具会安装 `oneui.uuid` 和 `oneui.native-sdk`，保存完整依赖锁，并生成项目 CMake 入口。该过程不构建 OneUI。

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyUuidApp LANGUAGES C CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/.oneui/plugins/windows-modern-x86_64-shared/OneUIProjectPlugins.cmake")
add_executable(my_uuid_app main.c)
set_target_properties(my_uuid_app PROPERTIES LINKER_LANGUAGE CXX)
oneui_target_plugins(my_uuid_app REGISTRAR register_uuid_plugins PLUGINS OneUI::uuid)
```

静态 Plugin SDK 将命令中的 `shared` 与 CMake 路径中的 `-shared` 一起改为 `static`。Windows 动态运行库会按目标依赖复制到可执行文件目录。

## 申请服务并生成 ID

```c
#include "oneui/plugins/uuid.h"
#include "oneui_generated_plugins.h"
#include <stdio.h>

#define CHECK(call) do { if ((call) != ONEUI_PLUGIN_OK) return 1; } while (0)
int main(void) {
    OneUiPluginRuntime *runtime = NULL;
    OneUiPluginLease *lease = NULL;
    OneUiPluginServiceV1 service;
    char uuid[ONEUI_UUID_BUFFER_SIZE];
    CHECK(oneui_plugin_runtime_create_v1(&runtime));
    CHECK(register_uuid_plugins(runtime));
    CHECK(oneui_plugin_runtime_start(runtime));
    CHECK(oneui_plugin_runtime_acquire_v1(runtime, ONEUI_UUID_SERVICE_V1, 1, &lease));
    CHECK(oneui_plugin_lease_service_v1(lease, &service));
    const OneUiUuidApiV1 *api = (const OneUiUuidApiV1 *)service.api;
    if (!api || api->abi_version != 1 || api->struct_size < sizeof(*api)) return 1;
    CHECK(api->generate(service.instance, uuid, sizeof(uuid)));
    puts(uuid);
    CHECK(oneui_plugin_lease_release(lease));
    CHECK(oneui_plugin_runtime_stop(runtime));
    CHECK(oneui_plugin_runtime_destroy(runtime));
    return 0;
}
```

在 Visual Studio 的 x64 开发者命令行中构建：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/my_uuid_app.exe
```

结果形如 `f54d6285-9bd7-4b9c-a2f0-54758971d709`。函数写入 36 个 ASCII 字符及末尾 NUL，因此缓冲区至少为 37 字节，内存由调用方持有。

## 线程、错误和关闭

在运行时创建线程调用生成函数。跨线程返回 `ONEUI_PLUGIN_WRONG_THREAD`；缓冲区过小返回 `ONEUI_PLUGIN_LIMIT`；无效指针返回 `ONEUI_PLUGIN_INVALID_ARGUMENT`；系统随机数服务失败返回 `ONEUI_PLUGIN_FAILED`。有可写空间时，错误会将输出首字节清空。

服务函数表和实例只在租约有效期间可用。持有租约时停止运行时返回 `BUSY`。先结束调用、释放租约，再停止并销毁运行时。完整错误清理可按应用的 RAII 或统一退出路径组织，不能跨租约保留实例指针。

## 离线恢复与移除

```powershell
oneui-site plugin restore --project . --target windows-modern-x86_64 --linkage shared --offline
oneui-site plugin remove --id oneui.uuid --project . --target windows-modern-x86_64 --linkage shared
```

离线恢复使用项目锁与已校验缓存；文件被改动时会报错。移除依赖后，也应从应用源码和 CMake 中移除对应服务调用与注册目标，再重新构建应用。

## 独立开发与源码

[UUID 插件源码](../plugins/uuid/CMakeLists.txt)包含公开 C 头、系统适配、包描述、CMake 安装配置和独立打包脚本。[消费工程](../examples/plugins/uuid_app/CMakeLists.txt)可复制到自己的仓库。构建插件时通过 `CMAKE_PREFIX_PATH` 提供已安装的 Plugin SDK；无需修改 OneUI 核心、注册器或官方包打包工具。

UUID 布局依据 [RFC 9562 第 5.4 节](https://www.rfc-editor.org/rfc/rfc9562.html#section-5.4)，系统随机数接口见 [BCryptGenRandom](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom) 与 [SecRandomCopyBytes](https://developer.apple.com/documentation/security/secrandomcopybytes(_:_:_:))。
