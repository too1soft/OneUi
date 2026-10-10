# WP4 S3–S8 产物审计

口径：仅 `E:/project/byname/oneui-views/build`，上限 30 GiB；主仓库其他会话目录不统计、不修改。
可读取逻辑占用 **8.477 GiB**；E 盘剩余 **55.88 GiB**。
8 个旧 viewc 临时目录拒绝访问，大小未知；统计不完整，未知值没有按零计算。

正式证据目录约 19.42 MiB；12 张真实截图加 1 张小型输入 PNG。
保留用户要求的五轮逐帧数据、20 次新进程记录、身份 CSV、验证输出及 manifest；不按截图复制 exe/DLL。
capture、cargo-target、codex-views 中当前对象和桥接库供最终正常终端复核使用。
固定构建助手暂留至该检查点复核；旧 benchmark/generated headers 不作为本次新增证据。

## 未完成的缓存清理

**自动审批审查拒绝**删除下列旧 ILK/重复对象的 PowerShell 操作（blocked by policy），未执行、未换工具绕过。
确认可再生且无当前测试/编译进程占用的待删文件共 100 个、1.853 GiB；本次释放量 **0**。
ILK 是当前 /INCREMENTAL:NO 不再使用的旧链接缓存；旧独立对象/测试 exe 已被 capture 当前目标取代。
手动处理前提：正常终端有权限，确认无其他构建/测试进程；仅删除本表文件，不整删目录。

| 绝对路径（按占用降序） | bytes |
|---|---:|
| [oneui_c_api_static_link_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_c_api_static_link_tests.ilk) | 111166816 |
| [oneui_terminal_raster_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_terminal_raster_tests.ilk) | 76484480 |
| [oneui_overlay_host_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_overlay_host_behavior_tests.ilk) | 75944496 |
| [oneui_time_series_chart_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_time_series_chart_behavior_tests.ilk) | 75719120 |
| [oneui_component_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_component_behavior_tests.ilk) | 75649344 |
| [oneui_scroll_view_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_scroll_view_behavior_tests.ilk) | 75645592 |
| [oneui_developer_tools_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_developer_tools_tests.ilk) | 75527152 |
| [oneui_native_menu_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_native_menu_tests.ilk) | 75473648 |
| [oneui_panel_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_panel_behavior_tests.ilk) | 75457504 |
| [oneui_overlay_host_lifetime_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_overlay_host_lifetime_tests.ilk) | 75188944 |
| [oneui_remote_input_region_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_remote_input_region_behavior_tests.ilk) | 75166072 |
| [oneui_slider_interaction_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_slider_interaction_tests.ilk) | 75118280 |
| [oneui_tree_view_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_tree_view_behavior_tests.ilk) | 75073936 |
| [oneui_selection_model_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_selection_model_tests.ilk) | 75040560 |
| [oneui_realtime_frame_view_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_realtime_frame_view_behavior_tests.ilk) | 75035048 |
| [oneui_portable_text_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_portable_text_tests.ilk) | 74931128 |
| [oneui_motion_diagnostics_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_motion_diagnostics_tests.ilk) | 74913936 |
| [oneui_interaction_static_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_interaction_static_tests.ilk) | 69221184 |
| [oneui_system_fonts_static_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_system_fonts_static_tests.ilk) | 69176440 |
| [oneui_unicode_conformance_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_unicode_conformance_tests.ilk) | 32157904 |
| [oneui_text_layout_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_text_layout_tests.ilk) | 31799648 |
| [performance_lab_motion_tests.obj](E:/project/byname/oneui-views/build/codex-views/performance_lab_motion_tests.obj) | 20887861 |
| [performance_lab_gallery_tests.obj](E:/project/byname/oneui-views/build/codex-views/performance_lab_gallery_tests.obj) | 20729047 |
| [performance_lab_authoring_tests.obj](E:/project/byname/oneui-views/build/codex-views/performance_lab_authoring_tests.obj) | 16766771 |
| [performance_lab_connections_tests.obj](E:/project/byname/oneui-views/build/codex-views/performance_lab_connections_tests.obj) | 16399934 |
| [declarative_tests.obj](E:/project/byname/oneui-views/build/codex-views/declarative_tests.obj) | 15947388 |
| [oneui_view_cpp_diff_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_view_cpp_diff_tests.ilk) | 13267400 |
| [reload.obj](E:/project/byname/oneui-views/build/codex-views/reload.obj) | 12296943 |
| [reload_test.exe](E:/project/byname/oneui-views/build/codex-views/reload_test.exe) | 12162048 |
| [runtime_test.exe](E:/project/byname/oneui-views/build/codex-views/runtime_test.exe) | 12123136 |
| [list_test.exe](E:/project/byname/oneui-views/build/codex-views/list_test.exe) | 12098560 |
| [terminal_workbench_main.obj](E:/project/byname/oneui-views/build/codex-views/terminal_workbench_main.obj) | 11915697 |
| [runtime_test.obj](E:/project/byname/oneui-views/build/codex-views/runtime_test.obj) | 11552965 |
| [table_record_tests.obj](E:/project/byname/oneui-views/build/codex-views/table_record_tests.obj) | 9496085 |
| [table_records.obj](E:/project/byname/oneui-views/build/codex-views/table_records.obj) | 8762948 |
| [reload_plan.obj](E:/project/byname/oneui-views/build/codex-views/reload_plan.obj) | 8571343 |
| [reload_test.obj](E:/project/byname/oneui-views/build/codex-views/reload_test.obj) | 8568071 |
| [oneui_win32_notification_activation_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_win32_notification_activation_tests.ilk) | 8533128 |
| [oneui_backend_contract_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_backend_contract_tests.ilk) | 8430000 |
| [list_test.obj](E:/project/byname/oneui-views/build/codex-views/list_test.obj) | 8409351 |
| [oneui_win32_window_loop_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_win32_window_loop_tests.ilk) | 8291344 |
| [oneui_c_api_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_c_api_behavior_tests.ilk) | 8088376 |
| [oneui_win32_accessibility_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_win32_accessibility_tests.ilk) | 7886384 |
| [oneui_application_lifecycle_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_application_lifecycle_tests.ilk) | 7863672 |
| [reload_properties.obj](E:/project/byname/oneui-views/build/codex-views/reload_properties.obj) | 7782222 |
| [oneui_title_bar_presentation_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_title_bar_presentation_tests.ilk) | 7640344 |
| [oneui_win32_dock_drag_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_win32_dock_drag_tests.ilk) | 7563960 |
| [compiler.obj](E:/project/byname/oneui-views/build/codex-views/compiler.obj) | 7498288 |
| [oneui_skia_path_cache_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_skia_path_cache_tests.ilk) | 7442312 |
| [cpp_mount.obj](E:/project/byname/oneui-views/build/codex-views/cpp_mount.obj) | 7152196 |
| [oneui_monitor_behavior_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_monitor_behavior_tests.ilk) | 7054624 |
| [oneui_reactive_lifetime_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_reactive_lifetime_tests.ilk) | 7028472 |
| [oneui_system_fonts_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_system_fonts_tests.ilk) | 6951856 |
| [list_delta.obj](E:/project/byname/oneui-views/build/codex-views/list_delta.obj) | 6910905 |
| [cpp_diff.obj](E:/project/byname/oneui-views/build/codex-views/cpp_diff.obj) | 6908885 |
| [oneui_interaction_c_api_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_interaction_c_api_tests.ilk) | 6822528 |
| [oneui_utf16_range_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_utf16_range_tests.ilk) | 6633128 |
| [oneui_c_api_view_reload.obj](E:/project/byname/oneui-views/build/codex-views/oneui_c_api_view_reload.obj) | 6594151 |
| [oneui_developer_tools_window_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_developer_tools_window_tests.ilk) | 6592344 |
| [oneui_native_menu_c_api_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_native_menu_c_api_tests.ilk) | 6580512 |
| [oneui_file_drop_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_file_drop_tests.ilk) | 6551280 |
| [oneui_frame_profile_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_frame_profile_tests.ilk) | 6495488 |
| [oneui_time_series_chart_c_api_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_time_series_chart_c_api_tests.ilk) | 6484512 |
| [oneui_c_api_workspace_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_c_api_workspace_tests.ilk) | 6484328 |
| [lower.obj](E:/project/byname/oneui-views/build/codex-views/lower.obj) | 5038085 |
| [main.obj](E:/project/byname/oneui-views/build/codex-views/main.obj) | 4722904 |
| [performance_lab_effects_tests.obj](E:/project/byname/oneui-views/build/codex-views/performance_lab_effects_tests.obj) | 4666660 |
| [ir_validate.obj](E:/project/byname/oneui-views/build/codex-views/ir_validate.obj) | 4219881 |
| [component_contract.obj](E:/project/byname/oneui-views/build/codex-views/component_contract.obj) | 4065781 |
| [oneui_win32_compat_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_win32_compat_tests.ilk) | 3573984 |
| [oneui_win32_window_shadow_tests.ilk](E:/project/byname/oneui-views/build/capture/oneui_win32_window_shadow_tests.ilk) | 3420672 |
| [schema_parser.obj](E:/project/byname/oneui-views/build/codex-views/schema_parser.obj) | 3123684 |
| [ir_graph.obj](E:/project/byname/oneui-views/build/codex-views/ir_graph.obj) | 3095139 |
| [validate.obj](E:/project/byname/oneui-views/build/codex-views/validate.obj) | 3012095 |
| [composition_references.obj](E:/project/byname/oneui-views/build/codex-views/composition_references.obj) | 2747651 |
| [schema.obj](E:/project/byname/oneui-views/build/codex-views/schema.obj) | 2632561 |
| [ir_reader.obj](E:/project/byname/oneui-views/build/codex-views/ir_reader.obj) | 2018393 |
| [emit_cpp.obj](E:/project/byname/oneui-views/build/codex-views/emit_cpp.obj) | 2017537 |
| [ir_writer.obj](E:/project/byname/oneui-views/build/codex-views/ir_writer.obj) | 1665835 |
| [lower_attributes.obj](E:/project/byname/oneui-views/build/codex-views/lower_attributes.obj) | 1663061 |
| [table_rows.obj](E:/project/byname/oneui-views/build/codex-views/table_rows.obj) | 1585838 |
| [emit_rust.obj](E:/project/byname/oneui-views/build/codex-views/emit_rust.obj) | 1458555 |
| [performance_lab_authoring_tests.exe](E:/project/byname/oneui-views/build/codex-views/performance_lab_authoring_tests.exe) | 849920 |
| [performance_lab_motion_tests.exe](E:/project/byname/oneui-views/build/codex-views/performance_lab_motion_tests.exe) | 808960 |
| [performance_lab_connections_tests.exe](E:/project/byname/oneui-views/build/codex-views/performance_lab_connections_tests.exe) | 806400 |
| [declarative_tests.exe](E:/project/byname/oneui-views/build/codex-views/declarative_tests.exe) | 801280 |
| [performance_lab_gallery_tests.exe](E:/project/byname/oneui-views/build/codex-views/performance_lab_gallery_tests.exe) | 789504 |
| [text_field_composition.obj](E:/project/byname/oneui-views/build/codex-views/text_field_composition.obj) | 787511 |
| [oneui-viewc.exe](E:/project/byname/oneui-views/build/codex-views/oneui-viewc.exe) | 617984 |
| [binary.obj](E:/project/byname/oneui-views/build/codex-views/binary.obj) | 550379 |
| [performance_lab_main.obj](E:/project/byname/oneui-views/build/codex-views/performance_lab_main.obj) | 541452 |
| [performance_lab_effects_tests.exe](E:/project/byname/oneui-views/build/codex-views/performance_lab_effects_tests.exe) | 374272 |
| [cpp_diff.exe](E:/project/byname/oneui-views/build/codex-views/cpp_diff.exe) | 311296 |
| [list-link.rsp](E:/project/byname/oneui-views/build/codex-views/list-link.rsp) | 11283 |
| [views-link.rsp](E:/project/byname/oneui-views/build/codex-views/views-link.rsp) | 11156 |
| [list-static.rsp](E:/project/byname/oneui-views/build/codex-views/list-static.rsp) | 9497 |
| [static-link.rsp](E:/project/byname/oneui-views/build/codex-views/static-link.rsp) | 9358 |
| [runtime-test.rsp](E:/project/byname/oneui-views/build/codex-views/runtime-test.rsp) | 8021 |
| [list-test-link.rsp](E:/project/byname/oneui-views/build/codex-views/list-test-link.rsp) | 540 |
| [abi-link.rsp](E:/project/byname/oneui-views/build/codex-views/abi-link.rsp) | 429 |

## 拒绝访问的旧 fixture

以下同属过期 viewc 测试临时目录；读取即 WinError 5，无法确认当前内容大小。
先在正常终端恢复访问并确认无测试进程、内容仍是可再生 fixture，再删除；未声称已清理。

| 绝对路径 | 大小 |
|---|---|
| [viewc-_ztlfb0p](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-_ztlfb0p) | 未知 |
| [viewc-vpiz1tj5](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-vpiz1tj5) | 未知 |
| [viewc-rt5_3vb7](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-rt5_3vb7) | 未知 |
| [viewc-nrqse2kj](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-nrqse2kj) | 未知 |
| [viewc-n80amp14](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-n80amp14) | 未知 |
| [viewc-fr3kdbgg](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-fr3kdbgg) | 未知 |
| [viewc-akgnxd9z](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-akgnxd9z) | 未知 |
| [viewc-9qim1tkb](E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-9qim1tkb) | 未知 |
