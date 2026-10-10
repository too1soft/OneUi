# WP4 S7：声明式设置组件

状态：实现与组件测试通过；下一步 S8 综合示例及最终验收。C ABI 44 不变，没有新增导出。

## 交付

- FontPicker：VM 提供结构化字体列表与权威 selected。控件保留内部搜索/IME、弹层和候选高亮；可见行使用各自 family 预览，不创建每字体 Widget。不可用字体不发选择请求，已选字体被删仍保留名称，不反向清空配置。支持上下键、Enter、Esc、加载/错误/空态与重试。
- MediaPicker：选择、清除、取消均为语义意图。内部不启动平台文件选择、不导入、不删文件；loading 和 asset 控制按钮可用性。提供 empty/preview/error 命名槽和替代文本，文件名省略显示、tooltip 保留完整路径。
- MediaPreview：同一 ImageView 保留，source 变化取消前一代；任意线程完成先写待处理结果，UI dispatcher（可注入）或 UI 动画队列排空后才触碰 Widget/VM。销毁后的完成被弱寿命与取消标记拦截。fit、九宫格 position、opacity 及 empty/error 槽接入，失败发送 source/code/message。
- 默认媒体服务：一个工作线程、有界 64 项队列，清理已取消请求；仅本地静态文件，拒绝 URL/UNC 和动画图。输入上限 32 MiB、解码上限 16 MP；以规范路径、大小、mtime 为缓存键，最多 8 项/32 MiB。未引入网络下载、视频或业务文件服务。
- Slider：声明式 range/step/value、rw v-model 与 0/1/2/3 四阶段事件。程序赋值抑制 setter/interaction 回发；VM 可选择实时预览或最终提交。修正既有 Slider 对合法微小 range/step 的无条件 0.0001 放大，保留非法旧输入的保护行为。
- Card：复用既有原生控件并注册单内容根；样式 padding 现在参与子项几何和自然尺寸，背景/边框/圆角/阴影仍走 StyleBox。
- ThemeScope：固定 14 个公开语义颜色 token，严格 i64 RRGGBBAA。非法/重复批次保留上次主题；逐项 delta 只重算对应局部子树，空表继承父主题，支持嵌套。跳过 NativeHost，不改终端专属颜色或全局 theme。

组件注册、app-named schema projection、静态槽展开、事件类型和热重载接入在 views 层；字体与媒体业务状态仍由 VM 所有。`tests/workspace/Pickers.one/.oneschema` 覆盖所有新注册组件。

## 验证

| 项目 | 结果 |
|---|---|
| picker_tests | 4 组通过：1000 字体可见行上限/键盘/IME/缺失选择；媒体晚到、取消、销毁和 UI 排空；真实 PNG、失效路径与拒绝远程；12 套主题、单项失效、非法 token 与全局不变 |
| picker_runtime_tests | 通过：编译 IR + VM 投影/事件，Slider setter 一次、程序赋值无回发，局部 token delta、原生身份、reload 及 VM 租约平衡 |
| viewc workspace | 14/14，通过字体/颜色字段错型、Slider 事件错型、媒体槽错误与 Card 多根拒绝等正反例 |

改变声明式内部类布局后先 clean 已知对象，再按既有优化配置重编。SkCodec 新头文件需要 C++20，仅 media_loader.cpp 使用文件级编译选项；公开 SDK 头文件仍兼容 C++17。没有重新配置 build/capture。

尚不以这组测试替代 S8 的 Rust 同 IR、真实设置抽屉截图和全矩阵验收。媒体默认 UI 队列排空的无窗口测试使用注入完成及显式 tick；真实窗口展示在 S8 验证。MyStudio 未同步。
