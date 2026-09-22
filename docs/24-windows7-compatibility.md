# Windows 7 SP1 兼容路线

更新日期：2026-09-08。状态：兼容实现已落地，x86 原生自动验收通过，完整平台验收尚未完成。

## 范围与功能边界

本次只修改 OneUI，不替换已安装 SDK，不修改 WYC、GoGUI 或 OneUpdate。
保留同一套控件、布局、C++ API 和 C ABI，不维护另一套精简界面。
Windows 10/11 默认仍走现代构建；Win7 SP1 使用独立编译配置和独立输出。
OneUI 可运行不代表宿主 Go/Rust 运行时、更新器或第三方 DLL 自动兼容 Win7。

不能承诺“100% 无影响”。本轮通过行为回归约束影响范围；Win7 不具备的新系统能力
采用兼容回退，而非模拟操作系统原本不存在的能力，例如 Win7 使用系统 DPI，不支持每屏独立 DPI。
GPU 路径仍按实际系统/驱动能力选择，失败时保留软件渲染。没有删除现代平台渲染或输入能力。

## 构建配置

| 配置 | 默认工具链 | API 基线 | 默认输出 |
| --- | --- | --- | --- |
| `win10`（默认） | 当前安装的 MSVC / SDK | Windows 10 | 原有目录不变 |
| `win7` x86 / x64 | MSVC 14.44.35207 + SDK 10.0.22621.0，`/MT` | Win7 SP1 | 独立 `-win7` 目录 |

Skia 固定 `1f26101197bff9fcd939a791beb3094297436d59`，使用现有 SkParagraph、HarfBuzz、ICU
及完整 Unicode 排版路径。Win7 附加静态 FreeType 字体加载回退，不引入额外运行时 DLL。
配方保存在 `cmake/windows-build-profiles.json`。不能仅改 `WINVER` 或 EXE 版本头就宣称兼容。

```powershell
# x86；x64 使用 -TargetCpu x64 / -Arch x64 和独立输出目录。
.\scripts\build-skia-static.ps1 -TargetCpu x86 -MinimumWindows win7 `
  -OutDir D:\oneui-builds\skia-win7-x86 -Generate -Build
.\scripts\build-oneui-msvc-bundled.ps1 -Arch x86 -MinimumWindows win7 `
  -Configuration Release -SkiaOut D:\oneui-builds\skia-win7-x86 `
  -BuildDir D:\oneui-builds\oneui-win7-x86
```

首次准备源码依赖按 `docs/05-static-skia.md` 的 Fetch/SyncDeps 流程执行。
正式包保持 `ONEUI_ENABLE_TEST_FRAME_CAPTURE=OFF`；仅 QA 构建传 `-TestFrameCapture`。
工具链缺失时直接失败，不自动换成不兼容的新 CRT。Win7 的 Skia 必须从固定配方重建，
不能混用历史预编译库。中英文 MSVC 均使用已探测前缀的依赖跟踪包装器，避免 Ninja 漏掉头文件变更。

## 已修复的真实问题

| 问题 | 修复与验证方式 |
| --- | --- |
| MSVC 14.50 静态 CRT 引入 Win8 时间函数 | 独立固定 v143 工具链；保留旧构建作负面导入审计样本 |
| Win7 缺少 Shcore 时缩放固定为 100% | 读取实际系统 DPI；测试 96/120/144/192 DPI 和 API 缺失/失败回退 |
| 原版 SP1 不支持新 DLL 搜索参数 | DirectWrite 从绝对 System32 路径安全回退加载，不搜索当前目录 |
| Skia 旧 DirectWrite 分支丢失已找到字体 | 保留匹配结果；原版 SP1 系统中文无缺字断言和画面验证 |
| Win7 原生字体加载器拒绝部分新字体格式 | 原生优先，失败时使用静态 FreeType；测试彩色表情、应用内存字体注册及复杂文字 |
| 连续窗口消息导致动画定时器饥饿 | 有界消息批次并显式检查已就绪帧；连续消息压力下必须按时回调 |
| 中文编译器的头文件依赖未被 Ninja 识别 | 探测并规范化 showIncludes 前缀；真实对象依赖不再为零 |

Skia 补丁由 OneUI 维护，见 `third_party/patches/README.md`。脚本检查完整文件哈希和版本，
支持幂等重跑；部分补丁和未知本地修改均拒绝应用，不重置第三方源码。
补丁配方测试只修改独立临时 fixture。

## 导入审计与发布门禁

`scripts/audit-windows-runtime.py` 使用 PE 文件本身，检查 x86/x64 架构、子系统版本、
普通与延迟导入、序号导入、导出转发及随包依赖。Win7 参考必须来自同架构原版 SP1。
旧的“只导入系统 DLL”检查不再被当作 Win7 兼容证据。

```powershell
python scripts/audit-windows-runtime.py --capture-reference D:\reference\System32 `
  --label "Windows 7 SP1 x86 original" --architecture x86 --output D:\reference\x86.json
python scripts/audit-windows-runtime.py --directory D:\oneui-builds\oneui-win7-x86 `
  --reference D:\reference\x86.json --output D:\oneui-builds\import-audit.json
.\scripts\package-win7-sdk.ps1 -Arch x86 -BuildDir D:\oneui-builds\oneui-win7-x86 `
  -WindowsReference D:\reference\x86.json -OutputDirectory D:\packages\OneUI-Win7-x86-candidate
```

参考文件只保留导出元数据和哈希，不复制/分发 Windows 系统 DLL。x86 参考不能用于 x64。
打包拒绝覆盖旧包，拒绝 QA 帧导出构建。该脚本生成 DLL/导入库 SDK，不包含完整静态实现库；
静态宿主需用同一固定工具链从源代码链接整个依赖闭包，并重新审计最终程序。
新增完整 SDK 路线：CMake 输出 sdk-inputs-Release.json，由相邻 windows-compat 工程的
scripts/package-oneui-sdk.py 生成动态与静态双 SDK，包含 FreeType 及实际完整链接清单。
该输出按配置与架构隔离，不覆盖旧 DLL SDK。Rust 静态消费者传 ONEUI_SDK_DIR 与
锁定的 ONEUI_SDK_MANIFEST_SHA256；Win7 禁止回到手工库列表。
通用 PE 工具现由 windows-compat 集中维护，本仓库入口保留兼容包装，并先校验 compat.lock.json。
Win7 包包含 FreeType 等许可证；保留这些声明，尤其 FreeType FTL 的产品文档署名要求。

## 2026-09-08 验收状态

| 环境 / 检查 | 状态 |
| --- | --- |
| 原版 Win7 SP1 x86 6.1.7601，隔离 VirtualBox，网络关闭 | 正式构建 27 项原生自动检查通过 |
| 同环境 QA 构建 | 35 项通过，包含 100%/125%/150%/200% 测试缩放的 8 张导出帧 |
| Windows 10 19045，Win7 x86/x64 配置 | 编译与本机 CTest 回归通过 |
| Windows 10 19045，现代 MSVC 14.50 配置 | 29 项 CTest 通过，保留原工具链和 Skia 路线 |
| Win7 SP1 x86 导入审计 | 同架构原版系统参考验收；报告的 `runtimeTested=false` 只指静态审计本身 |
| Win7 SP1 x64 | 已编译；缺少同架构原版系统参考和可运行测试系统，尚未认证 |
| 真正系统 DPI 切换、跨屏、真实 GPU/驱动、系统 IME/无障碍辅助软件交互 | 待人工/硬件验收；测试缩放与合成事件不能替代 |

原生测试包括 C/C++ 调用、静态链接、控件行为、Unicode、剪贴板和托盘生命周期；
不把“托盘对象重复创建”写成“用户关闭恢复 100 次人工验收”。
虚拟机 3D 关闭，默认 GPU 尝试成功回退，不构成硬件 GPU 认证。
测试帧由 OneUI QA 接口导出，不是操作系统桌面截图。
现阶段只能称为兼容候选，不宣布所有功能在两种 Win7 架构上已完整验收。

继续验收需 Win7 SP1 x64 ISO/虚拟机，以及实际显示器/驱动与输入法测试。
不修改已有虚拟机；新建测试机保留独立磁盘和随机测试凭据，网络关闭。
