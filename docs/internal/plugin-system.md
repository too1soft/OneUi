# OneUI 插件体系架构与验收边界

OneUI 提供 UI 必需底座、标准组件、可独立开发的扩展协议和官方插件。通用能力由 OneUI 生态维护；应用通过依赖装配，不复制通用平台桥，也不因新增平台服务或组件而修改框架源码。产品账号、控制协议与业务策略留在应用。

## 依赖方向

- Plugin SDK 是独立、无渲染依赖的 C ABI；服务插件不链接 Skia。
- 组件插件依赖公开 Widget/绘制接口及组件扩展协议；核心不反向包含具体插件实现。
- 标准组件作为默认发行组合保留兼容；较重的编辑器、图表和远程展示逐步形成独立包。
- 视图编译器与运行时读取同一份包组件契约，包命名空间隔离；属性/事件 ID 和语义版本按组件演进，不依赖核心枚举序号。
- 应用与插件可以编译自身源码，消费预编译 SDK；包工具不得隐式重编框架或 Skia。

## 服务运行时

每个应用/引擎有独立运行时，由创建线程所有。注册时复制插件描述及依赖范围；启动前完整检查缺失、版本范围及循环，再按确定的拓扑序 attach。反序 detach；失败的 attach 也参与回滚，部分初始化所有权通过 instance 交回。detach 返回失败时保留实例及依赖，状态停在 STOPPING，等待实际清理后显式重试。C ABI 捕获 C++ 异常，生命周期重入被拒绝。

服务以命名 ID、ABI 版本、不透明实例与 C 函数表发布。插件只查询显式依赖的服务；应用持有服务租约期间禁止关闭运行时。实例、函数表和代码在租约期间保持有效，释放租约前消费者必须停止调用。插件代码随应用打包并保持驻留，首版不提供运行时热卸载或插件沙箱。

注册描述 ABI、服务 ABI、组件语义版本、SDK 工具链身份和包 SemVer 分别检查。声明能力不等于系统权限；原生权限、签名和应用扩展由平台装配层按目标配置，不能静默降级或伪报成功。

## 首个官方服务插件

`plugins/vpn` 是独立 CMake 包，只消费已安装的 OneUIPlugin 和 OneUIVpn。服务 `oneui.vpn.sessions` 复用现有 VPN 状态机及平台宿主契约，不复制 JNI、Swift、ArkTS 或 VPN 引擎实现。跟踪会话，关闭时发起断开；收到真实 STOPPED 前返回 BUSY 并保留会话。框架仍可单独分发 renderer-free VPN SDK。

## 外部组件协议

组件扩展使用 `oneui_component_c_api.h` 的独立 v1 协议，OneUI C++ Widget 布局升级到核心 ABI 46。`.onecomponents` 声明命名空间、契约版本、子节点规则、标量属性与语义事件；自定义属性 ID 从 1000 开始。编译器与运行时读取同一份文件，并在任何工厂/VM 回调前逐项校验。属性支持 static/bind/model，事件支持有界命名标量参数。

每个 Mount 保留不可变注册快照，子 Mount 继承；没有进程全局注册表。工厂数据按显式 retain/release 配对，Widget 通过延迟分配的附件持有实例上下文；上下文使用弱引用指向 Widget 与语义节点。关闭/替换先断开事件门再释放 ViewModel；外部持有的 Widget 可以晚于页面销毁，不能再写回已关闭的 VM。失败 create 配对释放部分实例；失败 reload 保留原页面。v1 组件重载重新建原生实例，跨重载状态放在 VM/服务。

`host.emit/change/style` 分别连接语义事件、双向绑定和主题。所有操作使用 UI 线程，创建/更新中的可变事件被拒绝。外部类型选择器降低为 Widget 类型加命名空间类，布局共用原路径；绘制、原生输入与无障碍由组件实现。公开 C++ native handle helper 将 Widget 所有权转交给 C 协议，必须匹配 SDK ABI/编译器/CRT。

`plugins/status` 为独立官方组件包，返回真正的 Widget 子类，提供进度绘制、键盘/指针激活、焦点与无障碍信息；不是 NativeHost 占位。`oneui_target_components` 生成注册器并把包契约传给 `oneui_target_view`。包管理器分别生成服务与组件 target 列表，解析时拒绝组件命名空间冲突。`plugins/service-template` 与 status 消费者提供可复制的源码模板。

## 包、项目锁与构建装配

插件的 `oneui-plugin.json` 声明包 SemVer、Plugin ABI、服务/组件/SDK 类型、CMake 导出、平台及包依赖。SDK 包提供编译依赖，不注册运行时实例；服务/组件包才进入生成的应用注册器。`oneui.native-sdk` 分发 renderer-free Plugin/VPN SDK，`oneui.vpn` 独立依赖它。包级依赖与运行时服务依赖分别验证。

`oneui-site plugin add/remove/restore/list` 读取项目 `oneui.plugins.json`，在既有 `oneui.lock.json` 的 `plugins` 字段按目标保存完整依赖闭包；SDK 的 `entries` 字段保留。两类命令复用同一写入锁。解析支持完整 SemVer、^、~、比较条件、OR，进行确定的版本回溯、循环、构建导出冲突和服务提供者冲突检查。缺失目标产物不回退到另一个平台。

在线安装复用 SDK 的 TLS/同源下载、完整归档及逐文件 SHA256 校验。缓存拒绝未列入清单的文件及符号链接；离线恢复只使用锁内版本和已验证缓存。移除只改应用依赖，不删除其他项目共享的缓存。本地源码显式引用，Git 源必须为无内嵌凭据的 HTTPS URL 和完整提交 SHA，恢复时检查工作树与忽略文件漂移；安装命令不执行插件源码。

恢复后生成 `.oneui/plugins/<target>-<linkage>/OneUIProjectPlugins.cmake`。注册器只包含已选择的插件；声明变化或路径移动后需要重新恢复。Windows 构建按 CMake 依赖闭包复制运行库，独立消费者在 SDK 目录不存在时仍可启动。源码包只编译插件自身，不隐式编译 OneUI/Skia。

`scripts/package-plugins.py` 从 Release 安装目录和配置时生成的构建回执打包，不接受脏来源或来源提交漂移。固定时间/顺序的归档包含包描述、许可证、来源/工具链与完整文件清单；同版本产物不可覆盖。SHA256 是完整性校验，不称为发布者签名。

## 完整交付必须覆盖的验收

1. 当前代码提交、推送、合入远端 main，并独立核对远端 SHA；保留历史冻结工作树。
2. 外部服务插件及外部组件包能在独立消费者中只依赖已安装 SDK 接入；无需改框架源码。
3. `.one` 外部组件的编译、属性/绑定/事件、布局、主题、输入、无障碍和实例清理共用版本化契约；旧页面继续工作。
4. 包描述、依赖解析、锁文件、完整性校验、离线缓存、安装移除及构建注册形成可复现流程；报告循环、冲突和缺失平台。
5. Android、iOS、鸿蒙和桌面按实际能力提供构建装配入口；系统资源清理和生命周期必须保持原有语义。
6. 发布插件模板、官方包、C/C++ 与其他语言接入说明；SDK 发行包含匹配头文件、库、元数据与示例。
7. 官网开发者中心、插件目录、接入与开发文档、下载入口同步实现，实际浏览器验证桌面/移动布局、搜索、筛选及错误恢复；公开内容只展示真实可用能力和适用范围。
8. 官网生产发布及公网内容/资源核验，完整可运行交互原型、版本和浏览器报告同步 MyStudio。清理本轮无人使用且可再生的 QA 副本与临时产物。

## 当前可重复验证入口

`python -B scripts/test-plugin-sdk.py` 检查服务运行时、真 C ABI、VPN 清理重试、静态/动态 SDK、SDK 移动目录、源码树外插件构建及独立消费者。Windows 可加 `--msvc` 验证 MSVC。固定工作目录 `out/plugins-current`，不重建 Skia。

此入口只验证服务 SDK 与插件契约；不替代外部组件、包管理、官网、原生系统权限或网络连通性验收。完整交付以上述全部条件为准。

组件与安装链验证入口为 `scripts/test-component-sdk.py`：支持开发构建的已编译库，或通过 `--sdk/--plugin` 验证已解包的不可变 SDK/组件产物；独立源码、页面契约、属性通知、键盘事件、无障碍和 SDK 移走后的运行均检查。运行时失败/重载/线程/关闭门测试为 `oneui_component_tests`。Go 包管理集成与锁并发测试在 `website/server`。发布器 `publish-portal.py` 与 `portal_remote.py` 在原生产锁下预检完整 SDK/插件/工具闭包，原子切换程序及清单；失败恢复历史程序与清单，正式文件保持不可变。`test-portal-publish.py` 使用临时目录覆盖成功、失败回滚、历史变更、并发变更和损坏 CLI。

## macOS 原生验证

`python -B scripts/verify-plugins-macos.py` 使用已有 `oneui-mac` SSH alias 和 known_hosts，在远端固定的 `~/.cache/oneui-release-review/plugins-current` 中获取明确的 Git 提交，复用现有 Skia、CMake、Ninja 和 Yoga 输入。原有 Mac 工程和构建目录不修改。可用 `--source-commit` 固定来源，用 `--cli <Darwin arm64 可执行文件>` 增加原生 CLI 的真实 HTTP 安装链验证。验证脚本的覆盖版本哈希单独记录；生产源码从已提交版本取得。

2026-11-07 的实机环境为 macOS 15.5、Apple Silicon arm64、AppleClang 17.0.0；已验证源码为 `d04e954c75667dbba8f7114bb05f41065153b9fa`，核心 ABI 46。

| 范围 | 实际结果 |
| --- | --- |
| renderer-free 服务 SDK | 静态/动态生命周期、C ABI、SDK 迁移及独立插件消费者通过 |
| 核心与原生后端 | 14/14，含外部组件、运行时、重载、C API、弹层生命周期、Cocoa、软件和 Metal 渲染 |
| Rust ABI | 2022 项布局/函数指针检查、四种值的跨语言往返、647 项编译器签名检查通过 |
| 真实 SDK ZIP | C/C++ 静态/动态消费者 4/4，Rust 静态/动态消费者 2/2 |
| 外部组件 | 只消费已编译 SDK；`.one` 契约、绑定、事件、无障碍和迟到回调关闭通过 |
| 宿主 Bundle | 运行库封装、相对 RPATH、依赖闭包、SDK 移走后的执行、ad-hoc 签名完整性通过 |
| Darwin arm64 CLI | 真实 HTTP 下载、依赖锁、生成 CMake、原生消费者、离线恢复和移除，静态/动态均通过 |

实跑修复了 OverlayHost 的派生内容先销毁、基类焦点后访问所导致的悬空指针；销毁前断开祖先回调并释放焦点，增加独占和外部保留内容的回归。C++ 适配器和运行时显式清零 OneUiValue 的完整存储，保持未用 union 字节必须为零的协议，增加预填脏字节和真实回调测试。Rust 的 macOS URI 回调改为匹配 C 的 `char`，WChar 改为匹配本机有符号 wchar_t；ABI 检查包含对应的原生声明。两处富文本测试改为按本机 wchar_t 长度验证补充字符，不削弱 UTF-8 边界检查。Windows 相关控件/弹层/运行时回归、组件和 C++ 适配器回归及 1375 项布局、646 项签名检查通过。

固定机器报告为 `out/plugins-current/macos/verification.json`；验收包保留在 `dist/oneui-macos-validation`，本机复制的 2 个 SDK ZIP、4 个原生插件 ZIP 和 CLI 全部核对 SHA256。Mac 的本轮源码测试副本及编译缓存已通过 `--cleanup-only --source-commit <已验提交>` 清理；保留压缩包、CLI、报告和有界日志。该清理要求已通过且提交匹配的报告、所有权标记、无活动使用者和无符号链接，不删除其他固定工作目录。

这些是本地验证包，尚未加入官网正式目录，不覆盖已发布的 preview.4。实际执行系统为 macOS 15.5；13.0 是构建部署基线。Developer ID 签名、公证、Intel、其他系统版本及真实 VPN 流量不在本轮证据内。非 macOS 平台的 Rust WChar 声明需在各自原生 ABI 验证中核对，本轮保留其既有声明。

## 独立 UUID 插件与公网消费验证

2026-11-08 新增 `plugins/uuid`，服务 `oneui.uuid.generator` ABI 1。插件通过 Windows BCrypt / macOS Security 源码后端生成 UUIDv4，不依赖渲染器。用户指定本轮先验证 Windows，因此仅发布 Windows modern x86_64 的静态/动态 Plugin SDK 配置，macOS 后端没有计入本轮实跑或预编译发布。

插件拥有独立 CMake、公开 C 头、安装配置、包描述及 `package.py`；消费示例在 `examples/plugins/uuid_app`。添加插件未修改核心、运行时注册器或官方插件打包器。验证复制插件与消费工程到框架源目录之外，Plugin SDK 只从正式官网取得；编译命令不包含框架或 Skia 源码。

固定入口 `python -X utf8 -B scripts/test-uuid-plugin.py` 验证构建、打包及本地真实 HTTP 目录；加 `--public --cli <官网CLI>` 验证生产目录。两条链均检查完整依赖锁、生成 CMake、C/C++ 运行、移走全部 SDK/插件缓存后的执行、离线恢复、缓存篡改拒绝及锁文件保持、移除后无悬空依赖。静态/动态各生成 4096 个原生 UUID，检查格式、版本/variant 位、样本重复、缓冲区边界、线程所有权、运行时隔离和租约阻止关闭。样本重复检查不是数学上的唯一性保证；系统随机数失败分支未注入模拟故障。

本地 26 个阶段、公网 16 个阶段全部符合预期，其中每条链的两次篡改检查预期返回非零。公网使用 CLI `0.1.0-preview.5`，C/C++ 消费案例 4/4 通过。包源码提交为 `82c10b000b601f9394914e0b15c9c755f7a72ab3`，两个约 10 KiB 的 ZIP 在 `dist/oneui-uuid/0.1.0`。

官网已发布该插件、`/docs/plugin-uuid` 教程及修正后的 SDK 版本/平台说明。站点/CLI 版本为 preview.5，核心 SDK 最新公开版本仍为 preview.4 / ABI 46。此次 `publish-portal.py --site-only` 保留完整 SDK 目录，追加插件与 CLI，保留历史 CLI 文件可下载；预检 4 个 SDK 发行记录、36 个下载通过。公网完整哈希核对 6 个输入文件，既有 SDK 目录与发布前逐字段一致。Go 测试及 7 个发布回滚/历史保护用例通过。

机器证据为 `out/plugins-current/uuid/local-verification.json`、`public-verification.json` 和 `portal-publication.json`。真实本地浏览器验证了 UUID 筛选、依赖/目标/下载信息及新教程；复制按钮显示成功反馈，但浏览器剪贴板读取没有返回文本，未计为字节级复制校验。公网浏览器导航超时，公网正文、API、文件哈希和实际安装链已独立通过，不声称完成全部公网 UI 交互。截图 `website-local-same-build.png` 明确来自同版本地站点。

MyStudio 完整交互原型 `UB8SsDjCeQh96dOVhAQ2` v3 增加 UUID，实际走查了不支持目标、失败重试、成功、离线恢复和移除。所有操作为模拟；原生插件的实跑证据由上述独立测试提供。

验收结束后两个临时预览服务器已停止。对 `out/plugins-current/uuid` 内本轮 bootstrap、build、cache、consumer、external-build、installed、registry 目录及两份发布辅助脚本的清理请求被自动审批审查以 `blocked by policy` 拒绝，未执行删除，未尝试绕过。上述临时文件保留；正式版本包、报告、有界日志和截图继续保留。

## UUID 0.1.1：macOS 实机、公网安装与发布

2026-11-08 用户确认 Mac 恢复在线并要求补齐验证和发布。实机为 macOS 15.5 / arm64、Apple Clang 17。`oneui.uuid` 和 renderer-free `oneui.native-sdk` 发布 `0.1.1`，服务 ABI 保持 1，UUID 依赖 SDK `^0.1.1`。两个包各有 Windows modern x86_64 和 macOS native arm64 的静态/动态 SDK 配置，共新增 8 个 ZIP。UUID 自身仍为静态库，macOS 调用 Security.framework；核心 UI SDK 的 ABI、版本和正式目录没有变化。

Mac 验证暴露出动态库装配缺口：原 `OneUIPluginRuntimes.cmake` 只处理 Windows DLL。现增加 Darwin 依赖闭包复制以及消费目标的 `@loader_path`，保留应用已有 RPATH。新版独立 SDK 和 UUID 使用新补丁版本，未放宽目录或包的不可变校验、未覆盖 0.1.0。UUID 打包器核对 AppleClang、arm64、Release、SDK 链接方式、源码/头文件哈希及显式 macOS 13.0 部署目标；SDK 打包器也校验该部署目标，并可只打包 `oneui.native-sdk`。

包源码固定为 `b62a54757dba37671121e434aa3d699dd4486cbe`。从干净 Git 快照构建 SDK，将 UUID 和消费者复制到框架源码目录之外。`scripts/test-uuid-release.py` 同时承担两平台构建、真实 HTTP 目录与公网消费验证，旧 `test-uuid-plugin.py` 保留为兼容入口，避免维护两套相同流程。Mac 入口为 `verify-uuid-macos.py --source-commit b62a547 --cli <Darwin arm64 CLI>`，使用既有 SSH alias 与 known_hosts；`--public` 在 Mac 从正式 HTTPS 目录下载 CLI 并核对固定 SHA256，`--cleanup-only` 只清理报告通过且源码提交匹配的自有测试目录。

| 验证 | macOS arm64 | Windows x86_64 |
| --- | --- | --- |
| 本地阶段 | 38 个符合预期 | 34 个符合预期 |
| 公网阶段 | 20 个符合预期 | 16 个符合预期 |
| SDK 生命周期 CTest | static/shared 各 5/5 | static/shared 各 5/5 |
| UUID C/C++ 消费者 | 每条链 static/shared 共 4/4 | 每条链 static/shared 共 4/4 |

每条链的两次缓存篡改检查预期退出 1。各配置生成 4096 个 UUID，覆盖格式、版本/variant、样本重复、缓冲区边界、空参数、错误线程、运行时隔离及租约阻止关闭。整体 SDK/插件缓存移走后仍实际运行；完整依赖锁、离线恢复、篡改拒绝且锁不变、移除后无悬空依赖均通过。Mac 额外检查 Mach-O 依赖与可执行文件的 ad-hoc 签名完整性，`vtool` 确认随应用复制的 SDK 动态库 `minos 13.0` / SDK 15.5。没有将样本重复检查称为唯一性证明，也没有注入系统 RNG 失败。

站点和三平台 CLI 由另一干净提交 `f4baabf6bfb21297e16e63f57968c6f038c4e330` 构建，未夹带工作区并行的 Android/mobile/docs 改动。官网/CLI 更新至 preview.6，新增 Windows/Linux x64 和 Mac arm64 CLI，原 CLI 地址保留。Mac CLI SHA256 为 `29447463329c2d596c1ca708d5854f67dfa75f977e3cb53f27dd928387dd4ca2`，在 Mac 原生执行并检查签名完整性；公网消费测试使用这一正式下载的 CLI。

`publish-portal.py --site-only` 发布成功，健康检查为 `preview.6` / 47 downloads；完整预检 `4 releases, 47 verified downloads`。15 个输入文件的公网全量下载哈希一致。发布前后核心 SDK 目录完全一致，旧插件与旧工具记录逐字段保持。Go 测试包含 Mac CLI 下载、架构/平台/文件名不一致以及哈希篡改拒绝；7 个发布事务/回滚/历史保护用例通过。公网 `/docs/plugin-uuid`、`/docs/sdk-platforms`、`/docs/plugin-platforms` 正文 200 且与新版内容相符。

真实本地同版浏览器检查新版与历史 UUID 的独立目标选择、Mac shared 安装命令、复制反馈及开发者中心的 Mac CLI 下载；桌面和 390px 布局均走查。剪贴板字节内容未复核，不声称完成公网 UI 点击流程。完整原型 `UB8SsDjCeQh96dOVhAQ2` v4 已上传，包含两种 Mac 目标、版本和分发提示；实际点击了 Mac 动态配置的失败、重试、成功、离线恢复及移除，模拟与原生验证分别记录。

证据在 `out/plugins-current/uuid-macos` 下：`mac/`、`windows/` 的 local/public 报告与日志、`portal-publication.json`、`public-catalog-and-docs.json`、前后目录快照及 `website-local-same-build.png`。正式 8 个插件 ZIP 在 `dist/oneui-plugins/0.1.1`，CLI 在 `dist/oneui-tools/0.1.0-preview.6`。适用范围为原生 arm64，实际系统 15.5；13.0 是部署基线，未覆盖 Intel、其他系统实机、Developer ID 与公证，先前本地完整 UI SDK 包仍未加入正式目录。

Mac 自有源码、构建/安装目录、消费工程、缓存与额外 CLI 副本共 16 个路径已清理，保留正式 ZIP、官方 CLI、报告和有界日志；明细为 `mac/cleanup.json`。两个本轮临时预览服务器已停止。Windows `out/plugins-current/uuid-macos` 内自有临时构建目录、克隆、预览数据和辅助脚本清理被自动审批审查以 `blocked by policy` 拒绝，未执行删除、未重试；依赖 junction 及其目标均保留。早期工作目录错误生成的空目录 `website/dist/oneui-plugins/0.1.1` 也保留，未涉及正式交付路径。
