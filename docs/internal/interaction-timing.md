# Windows 输入、目标内容与提交计时

`ONEUI_INTERACTION_TRACE=<CSV 路径>` 在启动前启用。默认关闭时不创建文件或后台线程。此诊断与旧的 `ONEUI_SCROLL_TRACE` 独立；旧 trace 的 `animation_frame_presented` 是回调与绘制返回，不能作为输入到显示器呈现的证据。

## 应用接入

`include/oneui/interaction_trace.h` 提供 C++ 接口，应用可在自己的 bridge 中包装，无需修改 OneUI C ABI：

```cpp
const auto operation = oneui::currentInteractionOperation();
oneui::markInteractionPhase(nullptr, operation, generation, "action_start");
oneui::markInteractionPhase(nullptr, operation, generation, "route_requested");
// 延迟任务仍捕获上面的 operation 和 generation。
oneui::markInteractionPhase(nullptr, operation, generation, "route_commit");
// 主内容已实际挂载、布局/状态 flush 完成后：
oneui::markInteractionPhase(nullptr, operation, generation, "route_ready");
```

操作 ID 在窗口消息循环取出鼠标按钮、滚轮或键盘消息之后、进入窗口过程之前创建。无参查询只在当前输入作用域返回 ID，离开后返回 0；因此必须在事件回调内捕获，不能在后续任务中重新查询。带窗口的查询返回该窗口最近的输入。传入 ID 0 不生成应用阶段。

`markInteractionPhase(nullptr, operation, ...)` 从有界的 4096 个操作历史查原 HWND，不根据后来输入猜测窗口；显式 HWND 也必须与该操作一致。原窗口销毁后记录失效。应用阶段还支持 `action_end` 与 `measurement_end`。所有调用都在拥有窗口的 UI 线程执行。

主窗口循环返回后、进程仍活着且尚未卸载 SDK 时，调用无 token 的 `finishInteractionTrace()`。它停止接收新事件、drain 后台队列、写 trace_end 并 join 写入线程；多窗口应用在整体主循环结束后统一调用。不要提前在窗口 close event 中调用而漏掉最后一帧。Windows 进程退出可能先终止工作线程，不能依赖 DLL 静态析构作为完整日志终态保证。

`route_requested` 不改变绘制身份；`route_commit` 设置目标 generation 并清除 ready。仅匹配同一操作、generation 的 `route_ready` 可置位。每个正常 `WM_PAINT` 在开始时固定身份和 ready，直到该次绘制所有阶段结束；旧页面反馈与 ready 前的绘制不会因为随后 ready 而被追认为目标内容。

## CSV 与阶段

列固定为：

```text
sequence,qpc_ticks,qpc_frequency,pid,tid,hwnd,operation_id,generation,phase,paint_sequence,ready,visible,foreground,backend,success,message,queue_age_ms,dropped
```

QPC 原始整数计数与启动时缓存的 frequency 允许跨进程转换到同一计时域。HWND 为十进制整数。visible 同时要求窗口可见且未最小化；foreground 为当时前台 HWND 匹配。success 为 1、0 或未知 -1。`queue_age_ms` 只在 `input_received` 有值，以实际 `MSG.time` 与 `GetTickCount()` 的 32 位无符号差处理回绕，其他阶段为 -1；该时钟精度低于 QPC，不能把它冒充亚毫秒测量。

OpenGL 顺序为 `paint_start → content_paint_end → submit_begin → submit_end → swap_begin → swap_return → paint_end`。软件顺序为 `paint_start → content_paint_end → blit_begin → blit_return → paint_end`。没有表面或有效脏区时只记录失败的 paint_end，不补造提交。SwapBuffers 失败也记 success=0，随后另一 paint 才可能降级为软件。后端按每阶段实际路径记录，paint_start 为 unknown。

`content_paint_end` 包含内容绘制和开发 overlay；正常内容绘制中的 layout 可由现有 frame profiler 拆分。`submit_end` 在 SwapBuffers 之前。`swap_return` 紧接 API 返回；`paint_end` 还包含其后的驱动 census、IME 与诊断开销。不能忽略这部分 UI 线程占用。

写入线程采用 4096 条有界 pending 缓冲，单次最多接受 250000 条；溢出或达到输出上限增加 dropped。UI 线程只复制事件到缓冲，不打开或写文件。正常退出会 drain 并写 `trace_end`，其中含最终 dropped。缺少终态、dropped 非零、阶段缺失、窗口不匹配、非前台或 ready=false 都不能作为完整目标绘制样本。

这些阶段只证明输入接收至客户端提交 API 返回。物理屏幕仍需同一 QPC 域的真实前台输入与桌面画面采样或平台呈现事件旁证；SwapBuffers 返回、离屏打印、rAF、WGC/截图采样都不单独等价于显示器 FPS。

Windows 时钟依据：[消息时间](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagetime)、[QPC frequency](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency)。

## 调度回归

单个 `Window::post` 消息内每轮最多运行 32 个回调或 4 ms 后让出窗口循环，保留 FIFO 与后续消息唤醒。让出时整个剩余 batch 以 O(1) 移到 deferred 队列前端，不逐项回填。执行中的 batch 保留在本地：嵌套 native loop 可执行后来新 post 的 batch，但不会提前重入外层 batch 剩余的回调，保持原有重入边界。让出时提交该窗口已有的待绘制区，防止低优先级的 WM_PAINT 被持续补充的 posted 消息饿死。单个应用回调本身不可抢占，长回调仍应由应用拆分或移至工作线程。重复 post 合并唤醒消息，嵌套消息循环中新增回调仍可触发独立唤醒。

`oneui_win32_window_loop_tests` 的 callback flood 测试验证批量后台回调尚未执行完时 frame 已获服务，FIFO 测试验证让出后新增回调不超越旧队列。Rust dispatcher 的日志配置只读取一次，禁用日志时不格式化调用位置或 trace 文本。

输入反馈复用高精度 frame 调度：鼠标 hover/down/up、键盘反馈产生的局部脏区先合并，`kOneUiFlushInvalidations` 暂缓提交，当前帧完成动画采样后统一绘制，避免每个输入消息在 UI 线程同步等待 SwapBuffers。已有局部失效不会被每个 handled click 扩大为整窗。合帧期间的 caret 位置也交由已有 paint 尾部刷新，避免提前用旧布局向 IME 往返定位；强制 IME 上下文更新不延迟。原生 live resize 与窗口状态切换仍可同步绘制，因为它们可能在操作系统模态消息循环内运行。静态绘制 flood 与 pointer burst 测试分别验证无动画时的绘制公平性，以及连续输入消息无同步 paint、随后一帧仅绘制一次。
