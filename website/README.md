# OneUI 官网与 SDK 下载

Nuxt 生成静态官网；Go 标准库服务内嵌网站，旁置 releases.json 与 SDK 压缩包。运行时不需要 Node.js 或源码仓库。

页面：概览、下载中心、完整 Markdown 文档、组件 API 参考、平台支持与部署说明。发布 API 和页面共用实际文件清单，服务启动时校验全部包的 SHA256。

## 维护入口

- app/pages：独立页面，组件和样式在 app/components 与 app/assets。
- scripts/prepare-docs.mjs：仓库 docs/*.md 转为可搜索、有锚点的文档快照；链接指向站内文档或源码。
- server：Go HTTP 服务、清单验证、下载客户端与测试。
- ../scripts/package-versioned-sdk.py：从冻结 SDK / 真实 POSIX 构建封装动态和静态包，不复制完整构建树。
- ../scripts/make-release-catalog.py：校验压缩包并生成官网清单。
- ../scripts/build-website.ps1：生成网站，测试和编译 Linux / Windows Go 程序。
- ../scripts/test-versioned-sdk.py：从真实压缩包编译、运行 C/C++ 消费者，完成后回收当前提取目录。

依赖由 package-lock.json 固定；首次运行 npm ci。构建执行：

```powershell
./scripts/build-website.ps1
```

在仓库根运行。SDK 包和清单应已在 dist/oneui-website，缺少时直接报错，不隐式重编 OneUI。

## 开发预览

在 website 下 npm run dev 只运行前端；下载 API 由 Go 程序提供。最终验证必须运行 dist/oneui-website 下的 Go 程序，而不能用开发服务器冒充部署验收。

最终输出固定在 dist/oneui-website，缓存固定在 out/portal-build。文档与页面将预览、稳定和未验证能力分开；不凭编译成功宣称平台全面验收。部署命令见 docs/portal-deployment.md。
