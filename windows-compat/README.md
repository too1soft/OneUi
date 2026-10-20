# Windows 兼容基础工程

状态：**工程实现与兼容候选验收中，不能据此宣称 WYC 或 OneUpdate 已完整支持 Win7。**

本工程维护统一工具链补丁、平台识别、PE 依赖审计、锁文件及发布契约。
业务程序、OneUI 的 Skia 补丁、OneUpdate 的安装事务各自由所属项目维护。

## 固定路线

一份业务代码；产品 `modern` / `legacy` 双构建；OneUpdate 共用 legacy x86 / x86_64 底座。
Go 首轮统一 1.25.14，现代版官方工具链、兼容版同源码加受控补丁。服务端不降级。

| 文档 | 用途 |
| --- | --- |
| [架构与职责](docs/architecture.md) | 各项目边界、版本与支持矩阵 |
| [构建接入](docs/builds.md) | 锁定输入、构建命令、后续项目接入 |
| [更新协议设计](docs/distribution-v2.md) | 已局部接入、待完整验收的兼容分流与签名契约 |
| [发布与回滚](docs/release-rollback.md) | 候选、正式发布门禁及部署顺序 |
| [原生验收](docs/native-acceptance.md) | 系统、业务、更新与信任验收要求 |
| [本轮实现记录](docs/progress-20260908.md) | 已实现、待实现、待验收及证据索引 |
| [收尾进展与阻塞](docs/progress-20260909.md) | 2.4.3 候选、局部测试、尚未完成的发布闭环 |

`distribution.py` 是带测试的分流规范；Go/Rust 的 OneUpdate 本地实现已经接入，**尚未部署生产或完成安装/更新全链路验收**。
`compat.py verify-manifest --release` 是本地阻断门禁，不代替发布服务端的验收、验签与权限控制。
目前保留 `verification: {}` 的候选包会被正式门禁拒绝，这是预期行为。

## 本地检查

```powershell
python -m unittest discover -s tests -v
python compat.py verify-manifest --root D:\candidate
# 只有完整验收证据及干净源码才可通过下一项：
python compat.py verify-manifest --root D:\candidate --release --evidence-root D:\acceptance
```

Python 和构建脚本只运行在开发／CI 主机，不要求用户的 Win7 安装 Python、新 PowerShell、VC 运行库或额外系统补丁。
本轮没有配置远程仓库、提交、上传制品或部署生产。
