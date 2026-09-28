# 从一个输入框开始

这个示例适合学习 OneUI：先看 22 行的 [hello.cpp](hello.cpp)，再看包含保存、验证和千行列表的完整页面。Windows 首次构建需先按[仓库 README](../../README.md#运行-demowindows)准备 Skia。

在仓库根目录执行：

```powershell
.\examples\declarative\build.ps1 -Test
.\examples\declarative\build\bin\oneui-hello.exe
.\examples\declarative\build\bin\oneui-details.exe
.\examples\declarative\run.ps1 -Code -Dev
.\examples\declarative\run.ps1 -Page list -Dark -Dev
```

不加 `-Code` 时使用 `.one` 模板；`-Build` 重新编译，`-Test` 编译并测试。直接运行 exe 时用 `--code`、`--dev`、`--dark`、`--page settings|list|detail`。窗口关闭即丢弃模拟配置，不访问网络、不保存真实连接凭据。

## 改哪个文件

| 想做的事 | 文件 |
|---|---|
| 最简单的 C++ 双向输入 | [hello.cpp](hello.cpp) |
| 独立详情页：自动分栏、长文字、底部操作与主题 | [details.cpp](details.cpp) |
| 用 C++ 增加字段、组合页面 | [manual.h](manual.h) |
| 用标签写同一页面 | [views/Demo.one](views/Demo.one) |
| 复用标题组件、默认和具名插槽 | [views/Header.one](views/Header.one) |
| 增加状态、校验和保存逻辑 | [vm.h](vm.h) |
| 修改颜色、间距和字号 | [theme.css](theme.css) 和模板内 `<style scoped>` |
| 启动窗口、绑定生命周期和热更新 | [main.cpp](main.cpp) |

`State` 是能通知界面的变量；`Computed` 是由其他变量计算出的值；`VmCommand` 是带有“能否执行／执行中／错误”状态的操作。ViewModel 就是把这些变量和操作放在一起的 C++ 类。界面只绑定它们，两种写法共享业务逻辑。

## 五分钟体验

1. 设置页修改名称、刷新间隔，输入 `0` 查看错误与保存按钮状态。
2. 保存后继续输入，观察保存完成仍保留新草稿。点击“模拟保存失败”后再保存，查看失败与重试。
3. 切换主题、缩窄窗口、用 Tab 移动焦点；表单会换行，正文可滚动。
4. 列表页搜索 1,000 条固定数据，筛选、添加、选择、删除、查看详情；无匹配时出现空状态。刷新是约 800ms 的本地任务。
5. 用 `-Dev` 启动，修改 `theme.css` 中的字号；保存后经过 200ms 防抖更新。拼错属性时保留上次有效样式并显示诊断。修正或删除规则也生效，输入内容不丢失。

仅 CSS 可热更新。新增组件、改模板结构／事件／C++ 逻辑后，需要关闭应用并用 `-Build` 重编译。开发模式还将布局诊断写入 `artifacts/layout-current.txt`；正常滚动不报越界。

## 用到自己的项目

最省事的方式是复制本目录的完整源码（不复制 build/artifacts），保留 CMake 与构建脚本，再修改 `hello.cpp` 或完整页面。目录在仓库外时运行 `build.ps1 -OneUiRoot C:/你的路径/OneUi`；它会把 `ONEUI_SOURCE_ROOT` 指向 SDK。C++17、`ONEUI_ENABLE_YOGA=ON` 和 `target_link_libraries(app PRIVATE oneui)` 是声明式入口的基本要求。使用模板时额外调用 `oneui_target_view`，生成的头文件由构建自动处理。只用 C++ 的目标不必添加 `.one` 文件。

完整[模板／绑定接口](../../docs/35-declarative-authoring-v1.md)、[页面与布局指南](../../docs/36-declarative-page-patterns.md)。本轮入口为 C++，不代表 Rust 已拥有等价模板接口。

下一步可按[页面组合教程](../../docs/48-connection-page-recipes.md)体验性能实验台中的完整列表 → 详情 → 编辑流程；它与这里的最小程序使用相同的公共布局组件。
