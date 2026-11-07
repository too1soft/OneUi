# 开发原生服务插件

服务插件封装平台服务、设备能力或与界面无关的公共功能。它只依赖独立 `OneUIPlugin` SDK，无需链接 OneUI 渲染器或 Skia。每个应用创建自己的运行时，以版本化 C 函数表提供服务。

## 从模板开始

复制 [plugins/service-template](../plugins/service-template/CMakeLists.txt) 到独立仓库，修改 `example.counter`、服务 ID、入口符号和 CMake 导出。模板包含包描述、公开 C 头、生命周期实现和安装配置。它提供一个计数服务，便于先验证完整接入，再替换成真实能力。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/native-sdk -DCMAKE_INSTALL_PREFIX=/path/to/plugin
cmake --build build
cmake --install build
```

使用[包安装指南](plugin-packages.md)把插件添加到应用。应用调用生成的注册器，再启动运行时。实际 VPN 封装见 [plugins/vpn](../plugins/vpn/src/plugin.cpp)。

## 服务接口与版本

服务头只暴露 C 兼容类型、不透明实例和函数表。表首包含 `struct_size` 与 `abi_version`。申请服务租约时指定服务 ID 和 ABI，取得后再次检查函数表大小与版本。C++ STL 类型、异常和语言所有的字符串不穿过此边界。

包 SemVer 表达发布兼容范围；插件描述版本表达运行时依赖范围；服务 ABI 表达函数表兼容性。新增不兼容参数时发布新的服务 ABI。应用可同时锁定 SDK 和插件，更新插件不要求修改框架源码。

## attach 与 detach

运行时先校验完整依赖图，再按拓扑序调用 `attach`。在回调中通过 `context.publish` 发布函数表，通过 `context.dependency` 查询显式声明的依赖。不要保存 context 供异步调用。

初始化前设置 `*instance`，以便部分失败也可清理。每次尝试 attach 都会配对 detach。detach 成功意味着订阅、任务和原生资源全部释放；异步原生资源仍在关闭时返回 `BUSY` 并保留实例，处理真实平台回调后重试。不要把“已请求停止”当作“已停止”。

关闭顺序为：消费者取消工作和回调、释放服务租约、停止运行时、销毁运行时。存在服务租约时运行时拒绝停止。清理失败时保留依赖链，应用仍可继续完成清理。插件代码随应用驻留。

管理操作与模板服务在运行时创建线程调用。具体服务可以定义后台线程规则，但必须在自己的接口中说明调度、取消与回调所有权。跨线程消息与二进制大数据使用专用通道，控制命令不承担视频帧或大块内存传输。

## 平台与语言

系统权限由应用宿主申请，平台专用实现留在包内。桌面 DLL/so/dylib、Android JNI、iOS 原生宿主与鸿蒙 N-API 的装配方式见[平台装配](plugin-platforms.md)。包声明不会自动授予权限或创建平台签名。

C 和 C++ 直接包含 `oneui/plugin/c_api.h` 与插件服务头。Rust、Go、C#、Python 等可按头文件定义 `repr(C)`/FFI 结构与函数指针；对枚举使用对应整数宽度，显式检查返回值，保留回调到取消完成。所有语言都遵循同一租约和释放规则。

## 交付一个包

`oneui-plugin.json` 声明 ID、版本、Plugin ABI、服务类型、许可证、入口、CMake 导出、服务列表、平台和依赖。安装 CMake config、头文件与匹配库。每个目标分别构建并公布工具链要求；缺少目标产物时不替换为其他系统。先用源码树外消费者验证，再创建带逐文件 SHA256 清单的不可变归档。团队可使用自己的 HTTPS 包目录。
