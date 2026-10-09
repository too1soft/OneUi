# 维护 SDK 发布

业务项目统一从 https://oneui.too1.net/downloads 消费固定版本的预编译包。只有专门负责 OneUI 的会话可以构建框架和依赖；其他项目例外构建须先取得用户明确批准。固定构建目录及产物预算继续适用。

1. 在各原生构建环境完成同一源码版本与 ABI 的 Release 动态/静态库。Windows 输入为冻结 SDK（包含依赖清单和许可证）；Linux/Mac 输入为对应 Ninja 构建目录。不要混入别的版本头文件。
2. 使用 `scripts/package-versioned-sdk.py --help` 查看封装参数。Windows 传 `--input`，POSIX 传 `--build`；所有目标均传真实 `--source`、`--output`、`--licenses`、`--platform`、`--architecture`、`--profile`、`--toolchain`、`--minimum-os`、`--version`。POSIX 还需真实 `--source-commit`。脚本不编译库，拒绝覆盖已有 zip，输出两种链接包和目标 sidecar。
3. 六目标文件汇总到 `dist/oneui-website`。sidecar 可放独立构建记录目录；现有版本不得覆盖。实际消费者用 `test-versioned-sdk.py` 和 `test-versioned-rust-sdk.py` 从 zip 验证，更新 sidecar 中明确的验证环境及限制。
4. 运行 `python -B scripts/make-release-catalog.py --directory dist/oneui-website --records <sidecar目录> --version <新版本> --channel preview`。脚本校验包及内部清单，自动从清单提取 ABI 和来源，保留旧版本，原子更新清单。
5. 更新当前文档快照版本，再运行 `scripts/build-website.ps1 -Version <新版本>`。文档快照版本位于 `website/scripts/prepare-docs.mjs`；历史 SDK 内的文档随包永久保留。当前站内文档展示其明确版本，后续版本发布须先实现或维护对应站内文档快照，不能将旧版 API 静默标为新版。
6. 运行 Go `--check`、HTTP、浏览器与包消费者回归后交付或部署。每个已发布版本的包不可变；稳定版需完整原生验收及发布签名。候选替换开关仅适用于从未分发的本地预览元数据，不可用于覆盖已发布版本。

`build-website.ps1` 会先运行 `prepare-website-dlls.py`，从已校验的 Windows 动态 SDK ZIP 提取 `bin/oneui.dll`，按版本和目标命名并校验与包内清单一致。提取不重新编译库，也不改 ZIP；拒绝覆盖内容不同的现有 DLL。`runtime` 是清单 schema 1 的可选字段，旧 SDK 下载客户端继续忽略它并使用原 ZIP；发布时需同时更新网站程序和清单，旧网站服务会拒绝不认识的字段。

Go 编译没有第三方依赖；Nuxt 仅生成静态资源。程序同目录的普通文件不会被 HTTP 暴露，只有清单内的 ZIP 和 DLL 被列入下载白名单。交付说明、部署模板和校验总表随目录一同交付。
