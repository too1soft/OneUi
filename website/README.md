# OneUI 官方文档站点

这是一个独立 Nuxt 文档站点，只放在 `website/` 目录下，不修改 OneUI 现有项目源码。

## 技术栈

- Nuxt
- Nuxt UI

站点采用 Nuxt UI 的文档组件化写法，结构贴近 Nuxt 官方 Docs Template。Nuxt Content 当前未启用，因为当前机器安装 `better-sqlite3` 失败：预编译包下载超时，回退编译又缺少可用的 VS C++ build tools。后续如果要把章节拆成 Markdown 内容库，可以在修好这个本地构建依赖后重新接入。

## 本地运行

```powershell
cd website
npm install
npm run dev
```

默认访问：

```text
http://localhost:3000
```

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
