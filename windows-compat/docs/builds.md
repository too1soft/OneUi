# 构建与后续项目接入

## 精确输入

每个产品仓库提交 `compat.lock.json`，包含基础工程文件清单和各输入 manifest 的 SHA256。
本机绝对路径仅存在私有 `inputs.local.json`，不把开发机盘符提交为产品依赖。
输入 manifest 继续固定目录内每个文件，额外或损坏文件也会被拒绝。

本轮本机映射在 `D:\codex-builds\windows-compat-20260908\inputs.local.json`。
正式 CI 应从受控制品库恢复这些精确输入，不把本机该目录当长期分发源。

| 输入 | 首轮约束 |
| --- | --- |
| 官方 / 补丁 Go | 1.25.14，源码提交见 go-source.lock.json，GOTOOLCHAIN=local |
| Go x86 / x64 | GOARCH=386 + SSE2 / GOARCH=amd64 + GOAMD64=v1 |
| legacy MSVC / SDK | 14.44.35207 / 10.0.22621.0，静态 CRT |
| CGO | 锁定 MSVCRT 目标 MinGW-w64；与 OneUI 经 C ABI 连接，不混入 UCRT |
| OneUpdate Rust | 1.91.0，自行 build-std，i686-win7-windows-msvc / x86_64-win7-windows-msvc |

当前 Rust/MSVC 输入已有版本约束；完整 Rust 编译器、Windows SDK 和链接工具的逐文件溯源锁定仍待补齐，不能称为全链路可复现发布。

## Go 兼容配方

`scripts/build-go.ps1` 必须接收新的干净官方源码目录和明确 bootstrap，按 go-source.lock.json 审核补丁。
禁止重复把补丁施加到未知工作树。补丁由源码自行构建，不采用社区预编译 Go。

已拒绝社区补丁中的按路径重新打开 / 删除 os.Root 方案。原版 SP1 对目录操作的回退保持父目录句柄、重解析点检查及相对句柄枚举。
已修复 BCrypt 返回值应为 NTSTATUS，而非陈旧 GetLastError。
配方升级后先原生探针，再构建产品。

## WYC 四种构建

在 WYC 仓库执行；每次使用新的输出目录，旧包不会覆盖：

```powershell
.\scripts\build-wyc-windows.ps1 -Client gui -Profile legacy -Architecture x86 `
  -Version 2.4.2 -CompatRoot E:\project\byname\windows-compat `
  -InputsFile D:\codex-builds\windows-compat-20260908\inputs.local.json `
  -OutputDir D:\candidate\wyc-gui-2.4.2-legacy-x86 `
  -WindowsReference D:\reference\original-sp1-x86.json
```

将 Client 改为 `service` 即为无界面程序；Profile 可选 modern/legacy；Architecture 可选 x86/x86_64。
GUI 内置版本和 Windows 资源版本必须同步。旧 build-wyc-gui.ps1 仅作为受控入口别名，旧的 DLL 猜测参数已移除。
build-wyc-client.ps1 继续负责 Linux/macOS；Windows 必须使用上述独立入口。

编译、DLL、导入库和图标资源均检查架构一致。包包含 build-manifest.json；归档根目录直接包含程序文件。
GUI 候选包为普通便携包，不包含自动运行的 Agent，不应放入正式更新 Catalog。

## OneUI / OneUpdate

OneUI 配置生成 `sdk-inputs-Release.json`。执行基础工程的 `scripts/package-oneui-sdk.py --help` 查看明确的源码、构建、许可证和输出参数。
SDK 同时包含动态库、导入库、静态库闭包、头文件及许可证。legacy 闭包必须包含 FreeType。
OneUpdate 由 `scripts/build-windows.ps1` 接收 CompatRoot / InputsFile / OutputDir，构建两种架构。
Rust 消费者设置 `ONEUI_SDK_DIR` 与锁文件中的 `ONEUI_SDK_MANIFEST_SHA256`；绑定层再次检查 profile、架构、静态 CRT 和每个文件哈希。
原来未使用 ONEUI_SDK_DIR 的非 Win7 绑定调用方式保留；Win7 静态链接禁止手写库列表。

## 后续项目

| 类型 | 接入 |
| --- | --- |
| C/C++ GUI | include/windows_compat.h + 对应 OneUI SDK + 同架构 OneUpdate |
| Go GUI | 对应 Go/CGO + OneUI C ABI；x86 cdecl 必须用正确调用桥 |
| Go CLI | 对应 Go，CGO=0；runtime/go 提供系统识别，默认人工升级 |
| Rust GUI | Win7 target/build-std + 锁定 OneUI SDK + OneUpdate |
| 其他语言 | 先验证宿主运行时，不因 OneUI 可用而宣布支持 |

通用 C 和 Go 系统识别接口已实现；Rust 的通用识别接入示例及 OneUpdate 实际接入尚待完成。
native architecture 与 process architecture 是两个字段；RtlGetVersion 结果也不等于程序自身的架构。
