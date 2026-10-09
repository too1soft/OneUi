# 部署 OneUI 官网

官网由一个 Go 程序提供服务，页面、样式、脚本与文档已嵌入程序。运行机器不需要 Node.js、Go 编译器或 OneUI 开发工具链。

## 目录

```text
oneui-website/
  oneui-site-linux-amd64
  oneui-site-windows-amd64.exe
  releases.json
  oneui-0.1.0-preview.1-*.zip
  oneui-0.1.0-preview.1-windows-*.dll
  README.txt
  oneui-site.service
  oneui.too1.net.nginx.conf
```

选择对应系统的程序，其他程序文件可以不上传。保留 releases.json 引用的全部 SDK ZIP 和单独 DLL 文件。当前共 12 个 ZIP、4 个 DLL；只上传程序不能提供下载。不要把源码、私钥或其他业务数据放入分发目录。

正式域名确定为 `oneui.too1.net`，对外入口为 `https://oneui.too1.net/downloads`。域名已确定不代表已经上线；以下配置须在实际服务器安装并验证。

## Linux 启动

```sh
chmod +x oneui-site-linux-amd64
./oneui-site-linux-amd64 --check
./oneui-site-linux-amd64
```

Linux 默认监听 `0.0.0.0:23187`（所有 IPv4 网卡），可通过 `http://服务器IP:23187/downloads` 访问。`--listen` 可以覆盖监听地址和端口。

默认按可执行文件所在目录读取 releases.json 和 SDK，即使从其他工作目录启动也一致。使用 --data /absolute/path 可指定独立下载目录。

## Windows 预览

```powershell
.\oneui-site-windows-amd64.exe --check
.\oneui-site-windows-amd64.exe --listen 127.0.0.1:8080
```

浏览器打开 http://127.0.0.1:8080。该程序还支持 sdk fetch 命令，详见[接入指南](sdk-quick-start.md)。

## 公开服务

使用 Nginx、Caddy 等反向代理配置域名和 HTTPS，将请求转发到 `127.0.0.1:23187`。下载接口支持 HEAD、Range 和 ETag，可断点下载。大文件反向代理超时按下载带宽设置。

交付目录内附有 `oneui-site.service` 和 `oneui.too1.net.nginx.conf` 示例：

1. 把程序、清单和全部下载文件上传到 `/opt/oneui`，运行 `chmod +x /opt/oneui/oneui-site-linux-amd64` 及 `--check`。
2. 创建专用用户：`sudo useradd --system --no-create-home --shell /usr/sbin/nologin oneui`（已有用户则跳过）。确保该用户可以读取交付文件、进入 `/opt/oneui`；无需写权限。
3. 将服务示例复制到 `/etc/systemd/system/oneui-site.service`。执行 `sudo systemctl daemon-reload` 和 `sudo systemctl enable --now oneui-site`；用 `systemctl status oneui-site` 查看状态。
4. 设置域名 DNS 指向服务器；准备该域名的有效 TLS 证书。示例使用 `/etc/letsencrypt/live/oneui.too1.net/`，使用其他证书位置时需修改两条证书路径。
5. 将 Nginx 示例加入服务器现有配置。先 `sudo nginx -t`，通过后再 reload。示例是完整的 HTTP→HTTPS 跳转与 HTTPS 反代；证书未准备好时不要启用 TLS 配置。
6. 对外验证 `https://oneui.too1.net/healthz`，应显示 `downloads: 16`；访问 `/downloads`，下载一个 SDK 和一个 DLL，核对页面列出的 SHA256。

```sh
# 上线后执行；要求 DNS、证书和服务均已配置
curl --fail https://oneui.too1.net/healthz
curl --fail -I https://oneui.too1.net/files/oneui-0.1.0-preview.1-windows-modern-x86_64.dll
```

Go 程序不需要 root。使用 HTTPS 反代时对外开放 80/443；直接通过 IP 访问时开放 TCP 23187。不要用 Nginx 的目录列表或整个交付目录作为静态根目录；请求统一反代到 Go，保留下载白名单与完整性校验。

Linux 程序默认监听 `0.0.0.0:23187`；需要仅允许本机访问时使用 `--listen 127.0.0.1:23187`，并同步修改 systemd 的 ExecStart。没有后台管理登录、上传或在线执行接口；发布文件由站点管理员维护。

## 发布与维护

- --check 校验清单结构以及每个 ZIP、DLL 下载文件的大小和 SHA256，失败时非零退出。
- /healthz 返回服务版本和下载数量，适合进程健康检查。
- /api/releases 提供只读发布清单；/files/文件名 只提供白名单中的包。
- 文件不存在或完整性错误时启动失败；运行中下载文件被替换时拒绝提供新文件。
- 新增版本按[发布规则](sdk-release-guide.md)封装，更新清单后重启。不要覆盖历史版本。
- 升级时先上传新版本全部文件，再更新程序及清单，运行 `--check` 后重启。保留上一份程序和清单可用于回滚；回滚不得删除已对外发布的旧版本下载文件。
- 使用服务管理器收集 stdout/stderr，并设置日志轮转。服务不会记录令牌或 SDK 下载内容。

当前交付完成的是可部署程序与文件；域名解析、生产服务器进程和 HTTPS 配置需要在实际服务器部署后再验证。
