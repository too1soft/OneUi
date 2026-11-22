# 按应用裁剪静态运行时

OneUI 使用同一套主线源码构建完整 SDK 和按应用选择组件的静态运行时，不维护产品专用实现或分支。现有动态库、C/Rust ABI、OneView 二进制协议与默认完整运行时保持兼容。

## 静态对象与导出

DLL 与静态库分别编译 core、views、text 和平台对象。静态对象在编译时就定义 `ONEUI_STATIC`，不携带 `ONEUI_BUILDING_DLL`。仅在最终 `.lib` 上定义宏无法消除已经写入 DLL 对象的导出指令。

静态构建启用函数/数据分段与最终链接时的未引用代码删除。动态库继续使用原来的导出对象；不能用静态对象替换 DLL 对象而丢失既有导出。

## 自动收集模板依赖

源码方式集成 OneUI 后，按应用启用一次：

```cmake
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE OneUI::oneui_static)
oneui_target_view(my_app NAME Main SOURCE Main.one SCHEMA Main.oneschema)
oneui_target_view(my_app NAME Settings SOURCE Settings.one SCHEMA Settings.oneschema)
oneui_target_demand(my_app) # 放在全部视图声明之后
```

`oneui_target_view` 调用 viewc 时同时生成 `.requirements.json`。编译器收集导入模板、条件的各个分支、列表/插槽内容及复合组件的内部依赖。构建系统合并应用全部视图的清单，生成专用选择头文件并编译对应的 views 对象。未选择的组件分派和属性/事件路径在编译时消除；底层静态链接继续删除不可达代码。

这不是对业务源码做字符串扫描，也不从某次运行或某个分支推测完整需求。新增页面、修改导入或增添条件分支后，对应清单与选择头文件会随依赖自动更新。直接使用 C++/C/Rust 原生 API 的代码依赖仍由原生链接器保留。

## 动态内容与其他语言

运行时可能加载未参与本次编译的 IR、插件或动态组件时，应明确保留完整能力：

```cmake
oneui_target_demand(my_app FULL_RUNTIME)
# 或声明已知的动态入口。当前实现保守保留完整运行时。
oneui_target_demand(my_app DYNAMIC_COMPONENTS CodeEditor)
```

二者是替代用法，同一目标只调用一次。未知组件名会在构建时失败；精简运行时遇到未链接组件时明确报错，不会用普通容器静默代替。外部组件契约自动触发完整运行时回退，避免猜测原生提供器的内部依赖。

IR、C++、Rust 三种 viewc 输出均可同时生成同样的语言无关依赖清单：

```text
oneui-viewc --emit rust --input Main.one --schema Main.oneschema --output main.rs --requirements Main.requirements.json
```

其他构建系统可消费该清单；CMake 原生目标也可通过 `oneui_target_demand(target REQUIREMENTS path/to/Main.requirements.json)` 纳入外部生成的清单。此接口不会替 Rust Cargo 自动重建任意预编译 SDK。按应用重编 views 需要匹配的 OneUI 源码；只有预编译静态 SDK 的消费者仍受益于正常链接裁剪，但不能重新裁剪 SDK 中已经编译的通用工厂。

## 体积与性能

选择组件不会切换渲染器、降低分辨率、关闭动画或按静态文案缩减字形/分词数据。输入、文件名与服务端内容可以包含模板里未出现的文字。HarfBuzz、字体回退和当前渲染后端保持完整；ICU 默认完整，也可以按下面的显式服务配置构建。

源码应用可用 `-DONEUI_STATIC_IPO=ON` 启用当前工具链的整程序优化。它默认关闭，适用于使用同一兼容工具链编译、链接的应用；不能把特定编译器的中间代码静态包当成跨工具链通用 SDK。

验证使用 `oneui_demand_benchmark`，覆盖多语言文本、输入、复选框、进度、命令与响应式更新。`scripts/benchmark-demand.py` 保存重复测量和像素哈希；结果中 `startup_ms` 是进程内视图首次挂载耗时，不包含进程启动和首次绘制。光栅重绘结果不能代表 GPU 性能。完整验收与实测数据见同目录下的按需构建对比报告。

## ICU 与应用字体

Windows bundled-static 构建提供通用的文本服务配置，同一套源码同时支持 DLL 和静态库：

```text
-DONEUI_ICU_DATA_PROFILE=text
-DONEUI_LAZY_EMBEDDED_FONTS=ON
-DONEUI_STATIC_IPO=ON
```

`ONEUI_ICU_DATA_PROFILE` 默认 `full`。显式选择 `text` 时，保留全部断行/分词规则、全部相关语言资源和字典（包括 CJK、泰语、缅语、老挝语和高棉语），以及大小写、规范化、emoji 和文字布局数据。保留项按字节复制并重新校验。仅移除 OneUI 文字引擎不使用的排序、旧编码转换、货币/日历/数字格式化、地区显示名称等数据。它不是 ICU 全部公共服务的替代包；同一进程还需要这些 ICU 服务的消费者应保留 `full`。其他平台显式选择 `text` 会报错，避免静默不生效。

构建生成 `generated/icu-text.json`，记录输入、输出和每项保留资源的 SHA-256。`oneui_icu_data_profile_tests` 在独立进程比较完整包和文本包的语言相关断行与规范化；Unicode 官方用例继续覆盖完整文字引擎。

应用字体默认按首次使用的字重创建实例，字体字节、字符覆盖和九档字重均保留。只有单个 `wght` 轴且范围恰为 100–900 的字体启用延迟路径，其他字体沿用原有实例化方式。可用 `ONEUI_LAZY_EMBEDDED_FONTS=OFF` 回到预创建方式。`oneui_embedded_font_profile_tests <variable.ttf>` 比较实际字体各档样式、字形数、匹配结果与三种字号的逐像素结果。

SDK 打包器支持 `--linkage static`，不要求额外构建 DLL/viewc。清单记录 ICU、字体和 IPO 配置。Rust 读取 `staticIpo` 后使用 `static:-bundle`，把 MSVC `/GL` 归档留给最终 MSVC 链接器，避免 LLVM rlib 归档器丢失不透明对象的符号；因此最终链接时仍须提供对应 SDK。旧 SDK 没有此标志时保持原有行为。

真实 OneUpdate 对比使用 `rebase-oneupdate-test-bundle.py` 保持 V3 包的配置、签名计划、业务载荷、Agent、Logo 和尾部逐字节不变，只替换外壳。`measure-oneupdate-startup.py` 使用独立空目录，测量进程创建到安装器既有 `ui_window_visible` 日志点的耗时、CPU 和内存，不触发安装。`compare-oneupdate-frames.py` 使用安装器原有的预览/帧导出接口比较三套主题与六种状态；验证构建启用帧导出，生产体积构建关闭。帧导出采用光栅绘制，不能当作 GPU 截图或端到端安装验收。
