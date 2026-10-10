# WP4 S1–S2 产物审计与受阻清理

日期：2026-10-10。本轮预算仍是项目跨盘 **30 GiB**。没有重新配置 capture，没有新配置目录、截图、视频或交付二进制副本。只复用 capture、codex-views、cargo-target；直接链接禁止新增增量链接缓存。

最终本机五个根目录可读文件原始统计为 **32,731,569,658 bytes（30.484 GiB）**，E 盘剩余 **62,549,901,312 bytes（58.254 GiB）**。这个目录普查包括受保护的 Git/source archives 与其他任务产物，不能把整棵目录冒充可删除缓存；其中 `.pack` 单项约 6.33 GB。也不能据此声称项目产物已满足 30 GiB：未知 ACL 内容、远端输出尚未计入，分类尚不完整。

最初仅两个 build 根的普查为 25,068,417,690 bytes；结束审计扩大到了原仓库 out/Rust target/Skia out。因此前后数字口径不同，不把差额当作本轮新增量。原仓库 `out/artifact-state.json` 含移动端任务自己的 35 GiB 例外和 Mac/WSL 历史审计；本轮不继承该例外，不把历史远端数字当作新测量，也不修改这些目录。原工作区有他人的进行中工作，需由 Claude 在保留该约束下确认分类与清理窗口。

完整机器结果：[artifact-audit.json](artifact-audit.json)。本轮显式删除释放量为 **0**；直接非增量重链移除了部分对应旧链接状态，但没有计为人工清理成功。当前输出保留到 Claude 正常终端检查点；`build/artifact-state.json` 已覆盖为本轮状态。

## 原仓库受保护及当前输出（不是删除清单）

| 绝对路径 | 可读 bytes | 保留/待确认原因 |
|---|---:|---|
| E:/project/byname/oneui/build | 16,368,840,570 | 混有 Git/source archives、依赖与其他工作；原仓库只读 |
| E:/project/byname/oneui-views/build | 8,352,521,789 | 当前检查点及既有缓存；可清理的旧项另列 |
| E:/project/byname/oneui/out | 4,801,360,816 | 其他会话移动端/SDK 工作及历史证据，需确认占用与分类 |
| E:/project/byname/oneui/bindings/rust/target | 2,669,014,169 | 原仓库并行 Rust 构建缓存；本轮不触碰 |
| E:/project/byname/oneui/third_party/skia/out | 539,832,314 | 共享渲染依赖输出；不能作为废弃缓存删除 |

保留本轮固定 direct-MSVC 重放脚本、最终对象/DLL/static、测试 exe、当前 Cargo 缓存和小型诊断日志，供正常终端复核。编译器随机 fixtures 使用已有 finally 清理设施，本轮没有新增残留 fixture。正式证据为本目录 JSON/diff/Markdown，不属于临时截图缓存。

## 已确认可再生但清理受阻的文件

本轮针对旧 capture ILK 的原生 PowerShell 清理被自动审批明确拒绝：`blocked by policy`。没有换语言、换工具或关闭审批重试。历史 WP3 清单中仍存在的项目一并复核，保持其已有受阻结论。

手动处理前提：退出相关编译器、链接器、测试和占用这些路径的程序；由有权访问的正常终端用户核对具体路径后删除。仅处理下表文件/明确的诊断目录，不删除 capture 根、源码、Git、素材或正式交付。下表按大小降序，未知大小排最后，父子不重复计数。53 个当前 ILK 合计 2,272,490,984 bytes；全部已知大小受阻项合计 2,341,670,581 bytes。机器清单见 [blocked-cleanup.json](blocked-cleanup.json)。

<!-- blocked-table -->

| Absolute path | Bytes | Block reason |
|---|---:|---|
| [E:/project/byname/oneui-views/build/capture/oneui_c_api_static_link_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_c_api_static_link_tests.ilk>) | 108076832 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_control_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_control_behavior_tests.ilk>) | 79117240 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_terminal_raster_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_terminal_raster_tests.ilk>) | 75393976 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_overlay_host_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_overlay_host_behavior_tests.ilk>) | 74856104 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_yoga_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_yoga_behavior_tests.ilk>) | 74824944 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_terminal_view_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_terminal_view_behavior_tests.ilk>) | 74824072 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_interaction_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_interaction_tests.ilk>) | 74816416 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_authoring_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_authoring_behavior_tests.ilk>) | 74704152 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_background_opacity_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_background_opacity_tests.ilk>) | 74660800 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_time_series_chart_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_time_series_chart_behavior_tests.ilk>) | 74631120 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_component_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_component_behavior_tests.ilk>) | 74561008 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_scroll_view_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_scroll_view_behavior_tests.ilk>) | 74557136 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_developer_tools_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_developer_tools_tests.ilk>) | 74438976 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_native_menu_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_native_menu_tests.ilk>) | 74386448 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_panel_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_panel_behavior_tests.ilk>) | 74368896 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_style_scope_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_style_scope_tests.ilk>) | 74350992 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_stack_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_stack_behavior_tests.ilk>) | 74267056 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_overlay_host_lifetime_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_overlay_host_lifetime_tests.ilk>) | 74100512 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_remote_input_region_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_remote_input_region_behavior_tests.ilk>) | 74079304 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_slider_interaction_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_slider_interaction_tests.ilk>) | 74029904 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_tree_view_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_tree_view_behavior_tests.ilk>) | 73986024 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_selection_model_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_selection_model_tests.ilk>) | 73952232 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_realtime_frame_view_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_realtime_frame_view_behavior_tests.ilk>) | 73948600 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_portable_text_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_portable_text_tests.ilk>) | 73843696 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_motion_diagnostics_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_motion_diagnostics_tests.ilk>) | 73825504 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_interaction_static_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_interaction_static_tests.ilk>) | 69220408 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_system_fonts_static_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_system_fonts_static_tests.ilk>) | 69175224 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/wp3-obj](<E:/project/byname/oneui-views/build/capture/wp3-obj>) | 51450957 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/capture/oneui_unicode_conformance_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_unicode_conformance_tests.ilk>) | 32157904 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_text_layout_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_text_layout_tests.ilk>) | 31799648 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui-viewc.ilk](<E:/project/byname/oneui-views/build/capture/oneui-viewc.ilk>) | 11764776 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/wp3-menu-probe.exe](<E:/project/byname/oneui-views/build/capture/wp3-menu-probe.exe>) | 11385344 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/capture/oneui_win32_notification_activation_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_win32_notification_activation_tests.ilk>) | 8533128 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_backend_contract_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_backend_contract_tests.ilk>) | 8430000 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_win32_window_loop_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_win32_window_loop_tests.ilk>) | 8291344 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_c_api_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_c_api_behavior_tests.ilk>) | 8088376 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_win32_accessibility_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_win32_accessibility_tests.ilk>) | 7886384 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_application_lifecycle_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_application_lifecycle_tests.ilk>) | 7863672 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_title_bar_presentation_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_title_bar_presentation_tests.ilk>) | 7640344 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_win32_dock_drag_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_win32_dock_drag_tests.ilk>) | 7563960 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_skia_path_cache_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_skia_path_cache_tests.ilk>) | 7442312 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_dock_session_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_dock_session_tests.ilk>) | 7357832 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_monitor_behavior_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_monitor_behavior_tests.ilk>) | 7054624 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_reactive_lifetime_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_reactive_lifetime_tests.ilk>) | 7028472 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_system_fonts_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_system_fonts_tests.ilk>) | 6951856 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_interaction_c_api_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_interaction_c_api_tests.ilk>) | 6822528 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_utf16_range_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_utf16_range_tests.ilk>) | 6633128 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_developer_tools_window_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_developer_tools_window_tests.ilk>) | 6592344 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_native_menu_c_api_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_native_menu_c_api_tests.ilk>) | 6580512 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_file_drop_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_file_drop_tests.ilk>) | 6551280 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_frame_profile_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_frame_profile_tests.ilk>) | 6495488 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_time_series_chart_c_api_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_time_series_chart_c_api_tests.ilk>) | 6484512 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_c_api_workspace_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_c_api_workspace_tests.ilk>) | 6484328 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_win32_compat_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_win32_compat_tests.ilk>) | 3573984 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/oneui_win32_window_shadow_tests.ilk](<E:/project/byname/oneui-views/build/capture/oneui_win32_window_shadow_tests.ilk>) | 3420672 | Old incremental-link cache; auto-review blocked deletion |
| [E:/project/byname/oneui-views/build/capture/wp3-menu-probe.obj](<E:/project/byname/oneui-views/build/capture/wp3-menu-probe.obj>) | 2994878 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/codex-views/diagnostics](<E:/project/byname/oneui-views/build/codex-views/diagnostics>) | 1782302 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/codex-views/bench-smoke](<E:/project/byname/oneui-views/build/codex-views/bench-smoke>) | 1548880 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/capture/wp3-menu-probe.cpp](<E:/project/byname/oneui-views/build/capture/wp3-menu-probe.cpp>) | 9012 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/capture/wp3-menu-probe.rsp](<E:/project/byname/oneui-views/build/capture/wp3-menu-probe.rsp>) | 6504 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/capture/wp3-menu-probe.py](<E:/project/byname/oneui-views/build/capture/wp3-menu-probe.py>) | 1525 | Previously blocked WP3 cleanup; no retry |
| [E:/project/byname/oneui-views/build/capture/wp3-menu-probe.cmd](<E:/project/byname/oneui-views/build/capture/wp3-menu-probe.cmd>) | 195 | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmp0eqgtccj](<C:/Users/Administrator/AppData/Local/Temp/tmp0eqgtccj>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmp_i2t12bo](<C:/Users/Administrator/AppData/Local/Temp/tmp_i2t12bo>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpdhkrqr2i](<C:/Users/Administrator/AppData/Local/Temp/tmpdhkrqr2i>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpfffs0exo](<C:/Users/Administrator/AppData/Local/Temp/tmpfffs0exo>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpgqp61h6z](<C:/Users/Administrator/AppData/Local/Temp/tmpgqp61h6z>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpmdzriqng](<C:/Users/Administrator/AppData/Local/Temp/tmpmdzriqng>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpu1s95r9k](<C:/Users/Administrator/AppData/Local/Temp/tmpu1s95r9k>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpu5r44yvy](<C:/Users/Administrator/AppData/Local/Temp/tmpu5r44yvy>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmp1ets0lyf](<C:/Users/Administrator/AppData/Local/Temp/tmp1ets0lyf>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmp57ct9_8w](<C:/Users/Administrator/AppData/Local/Temp/tmp57ct9_8w>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmp97anzm5d](<C:/Users/Administrator/AppData/Local/Temp/tmp97anzm5d>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpiu8jdo0d](<C:/Users/Administrator/AppData/Local/Temp/tmpiu8jdo0d>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpnbx_c7al](<C:/Users/Administrator/AppData/Local/Temp/tmpnbx_c7al>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpo4466nc7](<C:/Users/Administrator/AppData/Local/Temp/tmpo4466nc7>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpxxi1v5kt](<C:/Users/Administrator/AppData/Local/Temp/tmpxxi1v5kt>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |
| [C:/Users/Administrator/AppData/Local/Temp/tmpzpmekavc](<C:/Users/Administrator/AppData/Local/Temp/tmpzpmekavc>) | unknown (ACL) | Previously blocked WP3 cleanup; no retry |

## Unreadable prior compiler fixtures

Sizes unknown; WinError 5. These are not counted as zero or as deleted. Normal-terminal ACL access is required.

- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-_ztlfb0p](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-_ztlfb0p>)
- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-vpiz1tj5](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-vpiz1tj5>)
- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-rt5_3vb7](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-rt5_3vb7>)
- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-nrqse2kj](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-nrqse2kj>)
- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-n80amp14](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-n80amp14>)
- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-fr3kdbgg](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-fr3kdbgg>)
- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-akgnxd9z](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-akgnxd9z>)
- [E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-9qim1tkb](<E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-9qim1tkb>)

The skipped original x11-test-tools symlink remains a protected/unknown entry; no link traversal or deletion was attempted.
