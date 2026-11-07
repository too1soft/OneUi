# 插件的平台装配

插件契约与平台系统权限分开管理。服务包消费独立 Plugin SDK；组件包消费匹配 OneUI SDK。CMake 工程只编译所选插件和应用，SDK 通过已安装包提供。

| 平台 | 应用装配入口 | 原生能力归属 |
| --- | --- | --- |
| Windows | CMake 导入库、生成注册器、依赖闭包复制 DLL | 插件管理自身句柄、订阅和原生回调 |
| Linux | CMake 静态/动态库、应用 rpath 或系统安装路径 | 应用提供桌面环境及包列出的系统库 |
| macOS | CMake 库、应用 Bundle 与签名 | 系统框架、entitlement 和应用扩展由宿主打包 |
| Android | NDK CMake 链接插件，JNI 适配层加入应用模块 | Java/Kotlin 宿主申请权限，Service 管理系统资源 |
| iOS | 原生 C/C++ 库随 App/Extension 链接，Swift/ObjC 宿主接入 | entitlement、扩展 Target 和系统授权由应用配置 |
| 鸿蒙 | OHOS CMake/N-API 模块链接，ArkTS 宿主注册 | Ability/ExtensionAbility 处理权限与生命周期 |

插件使用 `oneui_target_plugins` 生成服务注册函数，组件使用 `oneui_target_components` 生成组件注册函数；在应用/引擎实例建立时显式调用。注册器不依赖进程静态构造器。移动平台可在对应原生模块创建运行时，销毁时先结束原生资源和事件订阅。

原生 VPN 示例沿用 [VPN SDK](vpn-sdk.md) 的 Android VpnService、iOS Packet Tunnel 与鸿蒙 VPN 宿主桥。原生 SDK 包包含相关适配源码；添加这些源码到自己的应用 Target，并配置真实系统权限。隧道授权、路由、DNS、底层网络和系统停止回执仍通过原契约传递。

包目录中每个下载项写明具体目标、工具链、链接方式和许可证。源码支持平台与当前预编译目标分别列出；只有匹配目标的下载可以直接安装。组件的 C++ Widget 子类必须匹配 SDK ABI、编译器和 C++ 运行库。公开函数表使用 C ABI，但不能用它消除 C++ 控件布局差异。
