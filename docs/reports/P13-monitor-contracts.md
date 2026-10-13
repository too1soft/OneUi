# P13 monitor presentation contracts

Review requested; this report is implementation evidence, not owner acceptance or blueprint verification.

Row.class now supports VM bindings using the existing dynamic-class contract extension. A runtime test changes Row flow from row to column on a property notification while keeping the same controls. Progress accepts fill/track/radius CSS and applies the existing progress style adapter with native palette fallbacks. The sparkles name maps to the existing workbench glyph. Compiler tests exercise successful compilation and rejection of unconsumed padding.

Full capture ctest: 82/82 passed in all three integrated checks. The final check includes a clean rebuild, actual Progress paint assertions and prior real-pixel verification. The external windows-compat hash audit remains excluded by the existing check script. No public C++ class layout changed; reused build/capture without reconfigure. MSVC environment is initialized through vcvars64.bat before ninja.

The local Ninja header dependency gap required explicit owner timestamp refresh: component_contract.cpp, tools/viewc/compiler.cpp, src/views/format/ir_validate.cpp, src/views/format/validate.cpp, src/capi/oneui_c_api_view_mount.cpp. Final screenshots exposed stale inline StyleRegistry COMDAT code in other translation units despite correct resolved style metadata. The original capture directory was then cleaned once (461 files) and rebuilt; no extra build directory was created. A RecordingCanvas runtime test now verifies actual Progress fill, track and radius. Integration must rebuild both compiler and runtime because Row contract revision changes. Do not validate with a stale DLL/viewc.

Consumer: iShell P13 MonitorPanel/MonitorOverview. True native captures verified responsive Row layouts and restored 6px disk progress. Generic min/max are layout bounds, not Progress value limits; this is documented in the language-neutral views guide.

MyStudio project -l9ttZtpJcpAUs6fUrkp; task xR4UYVZ2UwH7cfp6ImiZ; execution rF7dOgu-2teHEBmvguTp. Local session 5c9d5c4d-7198-4a1f-b881-b40e14f4c8d2, agent /root/p12_p13. No confirmed blueprint scope is available, so no verified/acceptance claim is made.
