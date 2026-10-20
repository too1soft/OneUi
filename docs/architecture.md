# 架构与职责

## 不变约束

1. 兼容版不是旧版本冻结分支，同一产品版本使用相同业务代码。
2. 客户端支持目标是原版 Win7 SP1、Win8、Win8.1、Win10/11；每种系统、位数分别验收。
3. 不支持 XP、Vista、未安装 SP1 的 Win7；本轮不支持 Windows ARM64。
4. 不降低 TLS、签名或访问保护。系统缺少现代能力时做平台回退，不把失败当成功。
5. API、节点、OneUpdate 服务端和 Worker 保持原来的现代构建路线。

```text
windows-compat  工具链配方 / 锁文件 / 平台识别 / 发布门禁
├─ OneUI       modern、legacy × x86、x86_64；控件与 C ABI 共用
├─ OneUpdate   legacy × x86、x86_64；每应用隔离 Agent
└─ 产品
   ├─ WYC GUI  对应 Go + MSVCRT CGO + OneUI 动态 SDK + 同架构更新 FFI
   └─ WYC CLI  对应 Go，CGO=0，共用隧道核心，不安装自动更新 Agent
```

## 四个独立维度

| 维度 | 固定含义 |
| --- | --- |
| 业务版本 | 如 2.4.2；任何二进制修订正式发布均提高补丁版本 |
| profile | `modern` / `legacy`；不能用通道名代替 |
| architecture | 协议为 `x86` / `x86_64`；分别映射 Go 386 / amd64 |
| channel | `stable` / `beta` 等发布策略，与系统无关 |

同次业务发版可以有四种构建，分别拥有 build ID、文件哈希和验收证据。
不同构建不共用一个安装健康确认。已安装的配置与架构不得根据 OS 升级静默切换。

## 组件分工

- 基础工程维护通用 Go 兼容补丁；产品不复制补丁。
- OneUI 维护窗口、文本、绘图、输入、DPI、托盘及 Skia 专属补丁；SDK 输出由真实 CMake 依赖生成。
- OneUpdate 维护签名分流、安装、维护进程、激活、健康确认和回滚；底座不随产品 profile 分叉。
- WYC 维护登录、隧道、共享、日志、设置及服务实例业务；GUI 与 CLI 不复制协议核心。
- 下载网站只消费 OneUpdate Catalog；浏览器 UA 不作为最低系统的最终安装证据。

## 构建矩阵

| 组件 | modern x86/x64 | legacy x86/x64 |
| --- | --- | --- |
| OneUI 动态 / 静态 SDK | 均构建 | 均构建 |
| WYC GUI | 均构建 | 均构建 |
| WYC CLI / Windows 服务 | 均构建 | 均构建 |
| OneUpdate 安装器 / Agent / FFI | 不另建 modern 产品 | 两种位数供所有 Windows 使用 |

“构建成功”不等于“发布就绪”。Win7 x64 暂待用户另一台电脑原生验收；Win8/8.1 同样必须独立补齐。

## 工具链升级规则

Go、Rust、MSVC、SDK、MinGW、Skia、OneUI 或其系统调用依赖变更，必须重新生成锁文件并重跑受影响矩阵。
兼容修复未通过时暂停该兼容版本，不更换最低系统、不把官方 Go 混入 legacy 输出。
依赖的安全更新同时评估两条路线，不能以兼容为理由长期忽略漏洞。
