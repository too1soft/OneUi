# 官网 SDK 自动构建与发布

## 当前已上线版本

2026-10-13 已向 https://oneui.too1.net 发布 `0.1.0-preview.2`，ABI 44，Windows 10/11 x64：

| 下载 | 大小 | SHA256 |
|---|---:|---|
| 动态 SDK | 21,874,250 字节 | `30788c20fffec35d34a07f4a96ae5c9b92c51fe818747a1b258dffd1bc2962ca` |
| 静态 SDK | 85,101,401 字节 | `4ccdb13475b7bdd8cc15c5d7d8efd4c14af3d75b989fc1aa3d89c9bb07809d6d` |
| 单独 DLL | 15,504,896 字节 | `a839a49f42a80961d9612b955ae73d1534f3b7fec9b085db2e2e3df6234bfc7b` |

来源为干净提交 `f9f4b13615812079b2559468e1cc2a80162b8d13`，本机原始交付位于 `D:/oneui-sdk/dist`。新 DLL 与 `D:/oneui-sdk/build/oneui.dll` 一致。原来的 preview.1 全部 16 个下载继续保留；Linux、macOS、Win7、Windows x86 没有冒充新版本。

官网程序本身仍为 preview.1，因此 `/healthz.version` 仍显示该程序版本；SDK 版本看 `/api/releases`，本轮健康状态为 `ok`、19 个下载。内嵌文档快照未重建，版本说明明确指出接口以新包中的匹配头文件、绑定和清单为准。本轮发布不代表正式签名或全部原生平台验收。

## 服务器边界

`website/deploy/oneui-production.json` 是本机配置，不入库（已加入 `.gitignore`），只存非机密连接信息：`host`、`port`、`user`、`authentication`、`knownHosts`、`directory`、`container`、`containerDirectory`、`binary`、`healthUrl`、`publicUrl`。

- SSH：主机、端口和用户见上述本机配置；校验本机已有 `~/.ssh/known_hosts`。
- 1Panel 容器：`oneui`；宿主 `/apps/oneui` 绑定到容器 `/app`。
- 程序：`/apps/oneui/oneui`；监听端口 23187。
- 只重启 `oneui` 容器。不修改 1Panel、OpenResty、其他容器、数据库、端口映射和证书。
- `.oneui-publish` 只保留一个发布暂存和一份前次清单/校验清单；成功或正常失败后删除暂存。
- 旧的公开 SDK 文件永久按版本保留，不随回滚删除。无需上传 Windows 官网程序或重新编译 Go 官网。

## 发布已经编译好的 SDK

在 OneUI 源码根目录执行。需要 Python 3.11+、Paramiko（本机已有）。默认先给出计划，只有 `--apply` 执行上传和切换：

```powershell
python -B scripts/publish-sdk.py `
  --config website/deploy/oneui-production.json `
  --packages D:/oneui-sdk/dist --version 0.1.0-preview.2 `
  --targets windows-modern-x86_64

# 实际发布；同内容重复发布只核验，不重启、不覆盖
python -B scripts/publish-sdk.py `
  --config website/deploy/oneui-production.json `
  --packages D:/oneui-sdk/dist --version 0.1.0-preview.2 `
  --targets windows-modern-x86_64 --apply `
  --report out/native-release-review/publish-result.json
```

密码通过无回显提示读取；自动化可由机密管理器注入 `ONEUI_SSH_PASSWORD`，或用 `--password-stdin` 输入。不要把密码写进配置、命令参数、普通文件或 CI 日志。已有 SSH 密钥时可将配置改为 `authentication: key`，指定 `identityFile`；脚本不会自行创建或授权密钥。

## 构建、消费者检查、发布一条命令

入口是 `scripts/build-publish-sdk.ps1`。目前自动构建的目标是现有 **Windows modern x64** Release；发布器支持显式选择其他已有目标。Linux/macOS 等需要在对应构建环境先生成真实包，不能用这个 Windows 构建入口交叉冒充。

```powershell
.\scripts\build-publish-sdk.ps1 -Version 0.1.0-preview.3 `
  -Python 'C:/Users/Administrator/.pyenv/pyenv-win/versions/3.11.9/python.exe' `
  -CMake 'D:/Strawberry/c/bin/cmake.exe' `
  -ArtifactAudit out/native-release-review/publish-artifact-audit.json `
  -Publish
```

默认跨盘缓存预算 20 GiB。当前旧缓存总量超默认预算，所以新构建会被门禁拦住：先清理已确认且未被其他会话使用的旧缓存，或由用户明确批准本次例外后通过 `-ApprovedBudgetGiB` 传入；脚本不会自行提升。磁盘还要留足 22 GiB。**不能为了越过门禁少报输出根目录。** 审计文件 `roots` 必须覆盖当前项目全部已知受管输出（含并行会话新建目录），不能将历史清单永久视为完整盘点。

步骤：校验空间和干净源码 → 确认现有 CMake 缓存属于指定源码/配置 → 导入缓存指定的 MSVC → 增量构建 `oneui`、`oneui_static`、`oneui-viewc` → 生成完整冻结 SDK → 版本打包 → C/C++ 动态与静态消费者编译运行 → 发布前逐文件校验 → 上传/切换/公网验真。

固定目录：源码 `D:/oneui-sdk/src`、构建 `D:/oneui-sdk/build`、正式包 `D:/oneui-sdk/dist`、本轮临时冻结/消费者 `D:/oneui-sdk/release-current`。不会拉取代码、切分支或提交修改；维护者先准备要发布的源码版本。沿用当前已配置好的依赖，不创建新一套 Skia 构建。旧版本名不可重复打包；想再次核验同一包，使用 `-ExistingPackages -Publish`。

本轮实际跑通了**已有包的端到端发布**。完整编译入口已通过 PowerShell 解析、参数与工具链环境检查，未为了验证脚本而再次重编整套 SDK。未来首次完整构建仍须在清理/预算门禁通过后运行。

## 事务与回滚

1. 本机检查 ZIP、内嵌 manifest、每一个成员的哈希及实际目标；拒绝重复、目录穿越和混合来源。
2. SSH 只读核对实际 Docker 挂载、清单及可用空间，锁住发布事务并上传暂存。
3. 服务器复核历史记录不可修改，校验全部历史和新增文件，用现有 Go 程序 `--check` 校验暂存清单。
4. 原子更新 `releases.json` 和下载校验清单，只重启 `oneui`，检查本机健康与实际加载清单。
5. 从公开 HTTPS 下载新增每个 ZIP/DLL，流式计算 SHA256，不在本机保存重复包。健康或公网验证失败时尝试恢复上一清单；恢复失败明确报错，不宣称成功。

手动恢复最后一次清单：

```powershell
python -B scripts/publish-sdk.py --config website/deploy/oneui-production.json --rollback --apply
```

该操作只在当前清单哈希等于最后一次发布记录时允许，避免覆盖后来其他人发布的版本。强制断电/SSH 中断属于结果待核实：先检查官网及 `/apps/oneui/.oneui-publish/receipt.json`，不要盲目重试发布；若存在 `stage`，核对 `transaction.json` 所属事务、确认没有发布进程再清理。不得删除整个 `/apps/oneui`。本轮未在生产主动模拟宕机；失败回滚已通过本地文件事务回归测试。

## 本轮验证

- 12 项发布与版本历史回归通过：历史保留、拒绝覆盖/穿越/隐藏成员/损坏上传、并发清单冲突、重启失败恢复、自动回滚不得撤销另一发布者的后续版本、暂存清理。
- 新包全部 543 个清单成员校验通过；两包 source/ABI 相同。
- 现有构建的 authoring、静态 C ABI 链接、ABI 同步 3/3 通过。
- 服务器 Go `--check`、容器健康、公开清单及 3 个下载完整 SHA256 验证通过。
- 浏览器实际显示 preview.2、ABI44、动态 SDK/DLL 按钮和静态 SDK 按钮。
- 证据：`out/native-release-review/publish-result.json`、`publish-downloads.png`、`publish-artifact-audit.json`。

业务项目继续下载锁定版本；本脚本仅供负责 OneUI 的发布会话，其他项目构建 OneUI 仍需用户批准。
