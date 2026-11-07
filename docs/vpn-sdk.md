# 可选 VPN SDK

VPN 是独立于绘制引擎的 OneUI 系统服务模块。通过 `OneUI::vpn` / C ABI 管理配置、授权、连接状态、取消、清理和尝试代次；原生适配负责系统资源，包引擎负责加密与真实连通性。不依赖网云连、控制服务地址、账号、套餐或具体 WireGuard 协议。

## 归属边界

用户于 2026-11-04 明确要求：可以通用的能力必须进入 OneUI，业务应用不得另写一套。系统 VPN、权限、隧道资源、底层网络保护、生命周期、凭据存储和平台桥接属于 OneUI 的可选模块；本模块首先落实 VPN 契约。账号与设备归属、网络授权、控制协议、成员关系、ACL 与 Relay 业务策略由产品负责。平台桥接的后续接入也必须留在 OneUI，不能借“产品适配”把通用 JNI、N-API、IPC 或凭据实现搬回应用。

Android、iOS、原生 HarmonyOS 都是目标，不以 Android 兼容运行替代原生鸿蒙。具体产品的 HarmonyOS 7.0 实机目标不等于 SDK 已获该版本认证。

## API 与状态

头文件为 `oneui/vpn/c_api.h`、`oneui/vpn/session.h`。新增独立 ABI，不改已有桌面 ABI，不使桌面消费者自动链接 VPN。

`OneUiVpnConfigV1` 使用网络字节序 IPv4/IPv6、接口地址、规范网段路由、DNS 服务器与搜索域。标签须是合法 UTF-8；数量有界；默认路由须显式允许；路由的地址族须存在接口地址。`dns_domains` 是搜索后缀，不承诺三端都具备按域名分流的 DNS；需要分流时应在本 SDK 中补齐解析与平台适配，不能将 `addSearchDomain` 当作分流。

`oneui_vpn_config_json_v1` 生成三端一致的非机密配置，不包含登录令牌、设备凭据、WireGuard 私钥或 Relay 认证内容。传给系统存储的配置也只包含该非机密部分。

状态按 `idle → authorizing → establishing → tunnel-ready → connected` 推进。只有包引擎确认可服务，才能报告 `ENGINE_CONNECTED`。断开或取消进入 `stopping`，收到 `STOPPED` 后才能重新开始或销毁；错误状态同样必须等资源清理。旧 attempt 的事件被拒绝；所有调用在创建线程完成，后台和其他进程事件需经 OneUI 宿主投递回该线程。不要从 Host 回调中销毁会话。

Host `start` 必须在返回前复制 JSON 并迅速返回，不能阻塞 UI 等待网络；`stop` 对授权中、建立中和已连接都要有效。Host 必须报告实际事件；请求发出、启动扩展成功、系统 TUN 建立，都不等于引擎已经连接。

## 三端适配源码

| 平台 | SDK 源码 | 接入要点 |
| --- | --- | --- |
| Android | `platform/android/java/net/too1/oneui/vpn` | Activity 授权桥、通用 VpnService、前台通知接口、socket protect、FD 所有权、撤销与清理；manifest 不隐式启用 always-on |
| iOS | `platform/ios/vpn` | NETunnelProviderManager 控制器、Packet Tunnel Provider、公开 packetFlow 接口、配置和引擎就绪回执、并发清理合并；不通过私有 KVC 获取 utun FD |
| 原生 HarmonyOS | `platform/harmonyos/vpn` | Stage VpnExtensionAbility、create/protect/destroy、TUN 创建与销毁竞争、引擎失败、原生授权观察者；扩展的首次 Want 参数使用 API 22+，UI 观察者使用 API 26+ |

HarmonyOS 原生 `VpnConfig` 当前 MTU 上限为 1500，适配器显式拒绝更大值；不静默改包长。Android/iOS 也应通过目标系统验证其自身上限。包引擎必须在停止时释放数据包回调和工作线程，并使正在启动的调用结束；FD 复制与关闭的所有权见接口注释。

用户的 HarmonyOS 7 对应 API 26；当前原生授权观察者按该目标接入。官方配套为 DevEco Studio 26.0.0 / HarmonyOS 26.0.0 SDK，工具链版本依据[官方版本说明](https://developer.huawei.com/consumer/en/doc/harmonyos-releases/deveco-studio-new-features-2600)。当前环境尚未配置该 SDK，不能把源码接口核对当作鸿蒙编译通过。

Android 的 JNI、应用级会话、VPN 前台通知、socket 保护、物理接口枚举、系统密钥库、双向 FD 数据包输送与后台任务均由 OneUI 提供。iOS 对应能力包括 C ABI、Keychain、公开 packetFlow、NWPathMonitor/getifaddrs、VPN 会话及有界扩展消息。可选 Go 模块绑定真实 userspace WireGuard/mesh 引擎；其系统配置回调必须由原生 SDK 确认成功。

Android 的 `OneUiNetworkState` 仅从选中的非 VPN 底层网络读取 `LinkProperties`，在版本 1 快照中附加 `baseDNS.servers/searchDomains`。Go `NetworkAdapter.GetBaseConfig` 使用这份底层配置，拒绝空服务器、无效地址及指向本 VPN DNS 服务的循环配置；未提供该可选字段的平台仍返回明确的不支持错误。Android 系统不提供按域后缀配置的 DNS 分流，因此由真实包引擎代理内网域名，并将其他域名转发到底层解析器；不能关闭产品内网 DNS 来掩盖配置失败。底层 DNS 改变时重新编译解析配置，内容相同的系统 DNS 配置不重建 TUN。

`network_dns_test.go` 通过真实上游 DNS Manager 的 Android 非分流配置路径复现并覆盖此前的 `getting OS base config is not supported`，同时验证底层 DNS 更新与循环配置拒绝。消费产品在指定模拟器与现有 Windows 客户端完成真实双向 TCP、ICMP、系统内网域名/公网域名解析和断开后恢复验证；这些结果不代表已通过实体手机、锁屏、Wi-Fi/蜂窝切换、Private DNS 模式或其他平台验收。

Android 已有可供产品链接的预编译 OneUI 控件/绘制宿主 SDK，iOS 已编译 UIKit 绘制宿主、文本输入桥和静态 SDK。HarmonyOS 的 N-API、包泵、私有跨进程通道、安全存储、共享 Widget 光栅宿主和 IMEKit 源码已实现，并在 Linux 主机联合验证；尚未用 HarmonyOS SDK 编译，不是已交付的 HAP。官方下载页已实际跳转到华为账号登录，当前无账号和 SDK。

## 构建与消费

```sh
cmake -S . -B out/mobile-current/vpn-host -G Ninja \
  -DONEUI_VPN_ONLY=ON -DBUILD_TESTING=ON
cmake --build out/mobile-current/vpn-host
ctest --test-dir out/mobile-current/vpn-host --output-on-failure
cmake --install out/mobile-current/vpn-host --prefix out/mobile-current/vpn-sdk
```

与其他目标一起使用时设置 `ONEUI_BUILD_VPN=ON`。消费者使用 `find_package(OneUIVpn CONFIG REQUIRED)` 并链接 `OneUI::vpn`，匹配平台、编译器、C++ 运行时和静态/动态配置；不要在业务工程重编框架和 Skia。

固定目录的本机、Java 和 Android ARM64 验证入口为 `scripts/test-vpn-sdk.py`，Android SDK 与 NDK 通过参数指定。Apple 验证为 `scripts/verify-vpn-apple.py --ssh-host oneui-mac`；它使用已有 SSH alias 与 known_hosts，不读取密码，不安装 App，不启用 VPN。可选 `--product-root` 用于独立消费者测试，框架自身不依赖该产品仓库。成功后自动回收上传的源文件 fixture。

独立交互演示为 `docs/prototypes/vpn/index.html`，由 `scripts/bundle-vpn-prototype.py` 生成；只模拟流程，不访问生产接口。

## 本批验证边界

本机 C/C++ 测试、独立预编译 SDK 消费、Android Java 源码和 ARM64 静态库编译已通过。已有 Mac 实际运行契约与产品消费者测试，iOS device/simulator 静态库编译及 Swift 原生 API 类型检查已通过。浏览器实际走查了三端标签下的授权拒绝、恢复、取消、引擎失败和撤销。详细固定回执在 `out/mobile-current/vpn-host` 与消费产品的 `build/mobile-current`。

Android 已生成 arm64-v8a/x86_64 本地调试签名产品 APK，并在显式选定的 x86_64 模拟器启动、检查原生控件、表单校验、系统输入法和深色模式。内部集成 APK 已执行 Keystore 防篡改、FD 包输送、JNI/Go 业务桥、真实引擎启动和会话代次/清理序列。独立系统 VPN 检查使用 `tests/vpn/android` 与 `bindings/go/oneuivpn/cmd/vpnfixture`，通过真实授权拒绝/重试、系统 VPN 网络、两份不同系统 UDP 载荷经 TUN/WireGuard 加密与解密回包、页面退出后保持连接及清理；不使用诊断注包，测试对端仅在回环、临时密钥不记录，不随产品分发。消费产品脚本只打包此通用测试后端。它不替代生产组网和真机验收。

iOS device/simulator 的产品 App 与 Packet Tunnel 扩展曾链接，device 未签名、两者均未安装运行；缺 Apple 团队签名与可用模拟器 runtime。本批修正关闭失败时的资源保留与重试，`OneUiPacketEngine.stop` 通过 `Error?` 确认真实清理；这些 Swift 改动尚未重编。用户明确暂缓 Mac，最后一次 simulator 增量更新和源码 fixture 清理仍未确认。鸿蒙尚无 HAP。

HarmonyOS SDK 编译、三端真实 VPN 权限/网络切换/锁屏恢复/耗电测试、多网络单 TUN 分发与冲突处理仍是后续门槛。不得将浏览器原型或主机契约测试写成这些门槛已通过。

## 本批通用组件与主机验证

`scripts/verify-harmony-native-host.py` 通过 13 组检查，包含实际 N-API/C ABI、POSIX 包泵、回调超时/取消、有界调度、AES-GCM/私有原子文件、两个独立进程的 socket、实际 ArkTS RPC、C++ 会话驱动的 VPN 生命周期及资源关闭失败后的保留/重试。AssetStore、NetworkKit 授权/系统 TUN 和包引擎在涉及处明确模拟。真实主机进程通信不代表鸿蒙系统权限验收。

`scripts/verify-harmony-application-host.py` 用现有 Linux Skia 编译共享 Widget 光栅宿主。产品单独链接它，验证真实页面导航、UTF-16/中文预编辑、emoji、密码脱敏、旧会话拒绝、主题、密度及后台切换。实际 IME 适配代码运行，但 IMEKit 是测试替身；ArkUI/ImageKit 页面和 Stage 工程仍需真实 SDK 编译。生产 OHOS 配置要求有目标证明的 OHOS Skia，拒绝回落到 Linux/Android 库。

Android 两份 SDK 与通用 APK 已更新，五组运行时检查通过。新增检查通过真实 Go Host lookup 证明协议关闭失败后回调上下文仍在，成功重试后才释放。系统 VPN 检查通过实际授权拒绝/重试、两份不同内核 UDP 载荷的 WireGuard 往返、退出页面不断连及停止清理。启动期可能替换 netId，夹具选择带目标路由的最新 VPN 网络，在有界超时后重新绑定；没有诊断注包。Go 核心沿用与上次成功 APK 逐字节匹配的二进制，哈希和原始构建来源单列，不声称本批重新编译 Go 依赖。

鸿蒙通用能力都在 `platform/harmonyos`：

- `application`：Stage 窗口、ArkUI 呈现、触摸/取消、C++ Widget 工厂及 IMEKit 挂接/脱离；产品没有第二份 ArkUI 控件页面。
- `nativebridge`：动态模块、两个 Go 工作槽、有界任务/反序资源释放、同 UID 私有 socket 和版本化二进制 RPC。单消息至多 1 MiB，超时回复不生效，关闭等待实际处理完成。
- `storage`：AssetStore 设备本地 AES 密钥、AES-256-GCM、绑定命名空间/键名的 AAD；文件层仅保存密文，0700 目录、0600 文件、锁和原子替换，拒绝符号链接。
- `vpn`：C++ 会话作为扩展真实状态来源；认证 IPC 恢复状态，销毁界面不停止已授权连接。晚到授权/接口创建、替换回滚及关闭失败均保留所有权。主界面上下文不声称具有 VPN socket 保护能力。

`scripts/prepare-harmony-source-package.py` 只在固定目录准备 HAR **源码包**和原生类型声明，回执明确 `compiled_har=false`。产品消费真正的 `OneUI::harmony_application` / `OneUI::json` 预编译 SDK；缺库时不生成替代 `.so`。

Go Harmony 标签已排除桌面空保护实现，当前原生引擎故意返回 `ErrHarmonyRuntime`。拿到 SDK 后仍需构建固定来源的 OpenHarmony Go，审计运行时、网络系统调用和真实保护路径，再做手机验证；这不只是签名问题。官方声明中的 `VpnConnection.protectProcessNet()` 应纳入后续进程保护验证，不能用未经验证的空 hook 替代。

## 平台依据

- [Android VPN 接入](https://developer.android.com/develop/connectivity/vpn)
- [Apple Packet Tunnel Provider](https://developer.apple.com/documentation/networkextension/nepackettunnelprovider)
- [HarmonyOS VPN 增强管理](https://developer.huawei.com/consumer/cn/doc/harmonyos-references/js-apis-net-vpnextension)
- [OpenHarmony 官方接口定义](https://github.com/openharmony/interface_sdk-js/blob/master/api/@ohos.net.vpnExtension.d.ts)

## 新增可选组件

`OneUI::vpn_json` 提供 `oneui_vpn_connect_json_v1`，以严格有界 JSON 接入同一份 C++ 校验与状态机；Android JNI 与 Swift 共用，默认不改变旧 VPN SDK 依赖。启用 `ONEUI_VPN_JSON=ON` 后，消费者可用 `find_package(OneUIVpnJson CONFIG REQUIRED)`。

`oneui/mobile/application.h` 定义产品 Widget 工厂、请求/回复与视口/主题生命周期。Android `OneUiActivity`、UIKit `OneUiApplicationView` 负责平台绘制和输入。`NativeModule`/`OneUiNativeModule` 只输送有界业务字节，不识别产品账号。构建入口为 `scripts/build-android-application-sdk.py` 与 `scripts/build-ios-application-sdk.py`；产品只链接对应预编译 SDK。

当前 UIKit 使用 Skia 光栅与公开 CoreGraphics 展示路径；性能、VoiceOver、中文组合输入、密码输入及键盘避让仍需在已安装的 iOS App 中验收。Swift 类型检查不代表这些运行行为已通过。
