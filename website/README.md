# OneUI 官方文档站点

OneUI 官方文档站点，源码位于 `website/`。功能描述须与公开头、Rust bindings 和仓库文档同步。

## 技术栈

- Nuxt
- Nuxt UI

站点采用 Nuxt UI 文档组件。页面内容和组件/API 清单分别位于 `app/pages/index.vue` 与
`app/data/componentReference.ts`，当前不依赖 Nuxt Content。直接依赖及传递依赖通过
`package.json` / `package-lock.json` 固定，使用 `npm ci` 安装。
未使用的远程 Google 字体提供方已关闭，构建不需要请求其字体元数据。

## 本地运行

```powershell
cd website
npm ci
npm run dev
```

默认访问：

```text
http://localhost:3000
```

生产验证使用 `npm run build`。不要提交 `node_modules`、`.nuxt`、`.output` 或截图缓存。

## 平台状态维护

文字/命令/订阅升级尚未通过完整发布验收。内容与
[文字引擎状态](../docs/38-text-and-interaction-engine.md) 同步：Unicode 一致性补丁已回归通过，
MinGW 匹配文字依赖、SDK 审计和原生交互验收仍待完成，不把源码接入写成完整平台支持。

Linux/macOS 源码接入不代表已经通过原生验收。官网的支持矩阵与
[原生桌面后端状态](../docs/37-native-desktop-backends.md) 对齐，明确区分实现、构建与原生验收。
WSLg、交叉编译不能替代真实 macOS、国产发行版及 ARM64 桌面验证。

## 内容范围

- OneUI 当前定位与仓库结构
- 新手快速开始
- 核心 Widget/View/State/StyleSheet 模型
- 架构审查
- 构建、打包和运行时审计
- C++ API 与 C ABI
- 组件参考
- 样式系统
- 远程会话基础能力
- 当前测试状态
- 代码审查报告与后续建议
