# WP4 S5：Stack 与容器查询

状态：实现与本地验证完成。基线 b48df20；不修改 C ABI 44，不重新配置 build/capture。

## 实现

- Stack::insert/remove 仅为 C++ 能力。insert 的 index 是移除原位置后的目的序；同父 MOVE 保持 Widget、flex、焦点及 IME。跨父直接 insert 拒绝，显式 remove 后可插入；remove 不销毁外部持有的 Widget，先提交组合文本，再移焦点，并清理对应 flex。
- CSS 支持命名 size 容器、width/height/aspect-ratio、五种比较与 and。比值使用交叉乘法，H=0 不命中比例条件；无取整或默认滞回。解析失败不发布部分规则或 token，错误带 CSS 行列。
- size 隔离令外层 Yoga 把容器作为叶节点分配尺寸，子树不能反馈其测量尺寸。容器自己的第一轮布局后求值，条件变化时批量应用，再至多做一次该容器布局。稳定尺寸/样式/树版本直接命中缓存；不同尺寸但相同条件不重新应用样式。
- 查询测量 border-box 减边框、不减 padding。自查询及依赖式嵌套诊断后不应用查询；嵌套容器要求固定独立宽高。失效依赖不会靠每帧切换来“收敛”。Scroll 应放在独立测量壳内部，滚动条不能改变壳的已分配尺寸。
- display 与 authored visible 分开；ContainerPage 的 selected 是缺省 display，宽查询可显示多个 retained page，通用 visible=false 仍不可被 CSS 打开。查询切换不读取 VM、不增加 scope/content 租约。隐藏编辑项先提交 IME；CSS 删除/替换恢复缺省布局。
- 布局输出接入 Yoga/Legacy：方向、wrap（Yoga）、basis/grow/shrink、宽高及最小宽高；绘制样式复用现有适配器。纯解析/求值在 core，声明式节点与样式关联在 StyleRegistry。
- WSC1 新启用既定 bit 2（值 4）表示容器查询，reader 拒绝缺失能力标记的查询 IR；没有新增 C 导出或改变 VTable。

## 文件拆分

style_sheet.cpp 原 1,246 行，现 115 行。迁出 style_rule_parser.cpp 87、style_css_values.cpp 366、style_declarations.cpp 298、style_selectors.cpp 121、style_color.cpp 97、style_box.cpp 259。分别负责规则块、值、声明、选择器、颜色和绘制/合并。新增 Stack 操作在 stack_children.cpp（25 行），查询解析、求值和声明式应用各自独立。

严格规则校验发现原主题使用 `:pressed`，而 core 仅识别 `:active`；补充二者的同义映射，未删除主题规则或放宽未知状态校验。

## 验证结果

修改 Widget/Stack/StyleBox 的 C++ 布局后，先清除本工作树已知对象，再按原 RelWithDebInfo /O2 /DNDEBUG 命令重编。直接 MSVC 脚本的自定义测试依赖原先漏处理反斜杠路径，导致部分测试对象陈旧；已修正并重编，以下均为修复后的真实结果，不把初次挂起计作通过。

| 测试 | 结果 |
|---|---|
| container_query_tests | 5 组通过：精确 860/1100/1.6/H=0，坏语法，Stack MOVE/跨父/IME/焦点，Yoga 与 Legacy，最多两次布局，热更新/删除规则，依赖诊断 |
| container_runtime_tests | 通过：编译后的 .one/.oneschema、VM selected 切换、原生对象保留、查询期间无 VM 读取或新增租约、关闭租约平衡 |
| viewc workspace / binary | 12/12、11/11；含 required capability 与声明/事件签名负例 |
| Stack / Yoga / authoring / background opacity / style scope | 5 个程序均通过 |
| Pane interaction / motion / terminal / control / interaction / workspace tree | 6 个程序均通过 |
| Pane runtime / scope-host / WP3 runtime / list / reload / C ABI | 6 个程序均通过；runtime 9 场景、list 11+8 场景、reload 50+50 循环、C ABI 11 场景 |

共 19 个原生程序通过；不是全量 Ninja/CTest 的声明。固定构建目录和辅助脚本继续复用，最终 S8 统一检查产物与乱码。S8 还将扩展实际 Scroll/监控矩阵和截图证据，不用本阶段纯布局测试代替完整 A8 现场验证。

MyStudio 未同步；未调用需审批的写入工具。
