OneUI 官网与预编译 SDK · 0.1.0-preview.1
官网下载优化交付 · 2026-10-09
========================================

正式域名：oneui.too1.net
用户部署后入口：https://oneui.too1.net/downloads
此交付未执行公网部署。页面、样式与文档嵌入 Go 程序，服务器无需 Node.js、Go 或 OneUI 工具链。

上传到 Linux 服务器 /opt/oneui 的必需文件：
  oneui-site-linux-amd64
  releases.json
  全部 12 个 oneui-*.zip
  全部 4 个 oneui-*.dll
单独上传 Go 程序无法提供 SDK 下载。ZIP 与 DLL 留在程序旁，不需要解压。
oneui-site-windows-amd64.exe 仅供 Windows 本机预览，不需要上传 Linux。

Linux 启动：
  chmod +x /opt/oneui/oneui-site-linux-amd64
  /opt/oneui/oneui-site-linux-amd64 --check
  /opt/oneui/oneui-site-linux-amd64
--check 应输出 1 releases, 16 verified downloads。
Linux 默认监听 0.0.0.0:23187，可通过 http://服务器IP:23187/downloads 访问。
--listen 可覆盖默认地址，例如 --listen 0.0.0.0:23187。
程序默认读取自身目录，任意工作目录启动均可。--data 可指定独立下载目录。

后台服务与域名：
  创建专用 oneui 系统用户，确保它可以读取 /opt/oneui 中的文件。
  将附带 oneui-site.service 放到 /etc/systemd/system/。
  sudo systemctl daemon-reload
  sudo systemctl enable --now oneui-site
  sudo systemctl status oneui-site
  设置 oneui.too1.net DNS、准备有效 HTTPS 证书。
  oneui.too1.net.nginx.conf 是反向代理示例；按真实证书路径调整。
  sudo nginx -t 通过后再 reload。不要打开交付目录的静态文件列表。
  使用域名 HTTPS 反代时开放 80/443；直接通过 IP 访问时开放 TCP 23187。
  Nginx 示例转发到 127.0.0.1:23187。
  完整说明位于 DEPLOYMENT.md，或启动后的 /docs/portal-deployment。

Windows x64 预览：
  .\oneui-site-windows-amd64.exe --check
  .\oneui-site-windows-amd64.exe --listen 127.0.0.1:3187
  手动打开 http://127.0.0.1:3187/downloads

用户接入：
  首次接入下载完整 SDK，含匹配头文件、导入库/静态库、语言绑定、CMake、许可证和示例。
  Windows 动态版可单独下载 DLL；需重命名为 oneui.dll，与同版本同架构接入文件搭配。
  Linux .so、macOS .dylib 与静态库分别包含在对应 SDK 包中。
  /docs/sdk-quick-start 提供 C / C++ 接入，/docs/sdk-rust 提供 Rust 接入。
  业务项目仅编译自己的代码；其他项目构建 OneUI 必须取得用户明确批准。

自动下载：
  Go 程序自带 sdk fetch，可改名 oneui-site（Windows 为 oneui-site.exe）放入 PATH。
  官网部署后：
  oneui-site sdk fetch --url https://oneui.too1.net --version 0.1.0-preview.1 --target windows-modern-x86_64 --linkage shared
  同一用户的多个项目共享已校验缓存，各项目保留 oneui.lock.json。
  --offline 复用锁定缓存，升级须显式 --update。本地 HTTP 预览加 --allow-http。

校验与边界：
  --check 逐个校验清单内 ZIP/DLL 的大小和 SHA256。下载只暴露白名单文件。
  完整交付目录可运行 sha256sum -c SHA256SUMS；如未上传 Windows 程序，其缺失项可单独跳过。
  Windows 可用 Get-FileHash -Algorithm SHA256 <文件名>。
  本次 12 个 SDK ZIP 保持不变，4 DLL 从包内提取，没有重新编译框架或底层依赖。
  旧 SDK 的 24 次 C/C++ + 12 次 Rust 消费者运行通过，环境详见 verification.json。
  当前仍为预览版本：未正式签名或公证，Win7 与真实 Linux 桌面完整验收未完成。
  本轮浏览器工具被 URL 安全策略拦截，新筛选/下载按钮的浏览器交互验收尚待人工复核。
  服务器端自动测试与真实文件校验见 verification.json；不冒充公网部署或视觉验收。

升级先上传新版本全部文件，再更换程序和清单，--check 通过后重启。
历史发布 ZIP/DLL 不可覆盖；回滚程序和清单时保留已发布下载文件。
