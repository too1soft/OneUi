# WP3 list-performance artifact audit

Budget: 30 GiB through the S6 checkpoint, explicitly approved by Claude.

Readable build artifacts: 21,817,885,544 bytes (20.32 GiB).
Free E drive: 65,446,723,584 bytes. Eight prior ACL fixtures are unreadable; their sizes are unknown.

No new CMake configuration directory. capture cache was not reconfigured.
Retain capture, cargo-target and codex-views until normal-terminal final review.
The original OneUI worktree remains read-only; its build tree is a dependency input, not a deletion candidate.

## Retained output roots

| Absolute path | Readable bytes |
|---|---:|
| E:\project\byname\oneui\build | 16,368,840,570 |
| E:\project\byname\oneui-views\build | 5,449,044,974 |

## Blocked reproducible cleanup

Automatic approval review rejected deletion with `blocked by policy`; no alternate deletion tool was tried.
Manual deletion requires the relevant compiler/linker/test to be stopped and an account with access.
The two reused diagnostic directories are superseded by the final raw samples and counters in this report directory.

| Absolute path | Bytes | Reason |
|---|---:|---|
| E:\project\byname\oneui-views\build\capture\wp3-obj | 51450957 | automatic approval review: blocked by policy |
| E:/project/byname/oneui-views/build/capture/wp3-menu-probe.exe | 11385344 | Completed diagnostic copy; removal rejected by automatic approval review: blocked by policy |
| E:/project/byname/oneui-views/build/capture/wp3-menu-probe.obj | 2994878 | Completed diagnostic copy; removal rejected by automatic approval review: blocked by policy |
| E:/project/byname/oneui-views/build/codex-views/diagnostics | 1782302 | Superseded benchmark diagnostic artifacts; native PowerShell deletion rejected by automatic approval review (blocked by policy). No retry through another tool. |
| E:/project/byname/oneui-views/build/codex-views/bench-smoke | 1548880 | Superseded benchmark diagnostic artifacts; native PowerShell deletion rejected by automatic approval review (blocked by policy). No retry through another tool. |
| E:/project/byname/oneui-views/build/capture/wp3-menu-probe.cpp | 9012 | Completed diagnostic copy; removal rejected by automatic approval review: blocked by policy |
| E:/project/byname/oneui-views/build/capture/wp3-menu-probe.rsp | 6504 | Completed diagnostic copy; removal rejected by automatic approval review: blocked by policy |
| E:/project/byname/oneui-views/build/capture/wp3-menu-probe.py | 1525 | Completed diagnostic copy; removal rejected by automatic approval review: blocked by policy |
| E:/project/byname/oneui-views/build/capture/wp3-menu-probe.cmd | 195 | Completed diagnostic copy; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmp0eqgtccj | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmp_i2t12bo | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpdhkrqr2i | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpfffs0exo | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpgqp61h6z | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpmdzriqng | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpu1s95r9k | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpu5r44yvy | unknown | test fixture ACL; cleanup automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmp1ets0lyf | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmp57ct9_8w | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmp97anzm5d | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpiu8jdo0d | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpnbx_c7al | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpo4466nc7 | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpxxi1v5kt | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |
| C:/Users/Administrator/AppData/Local/Temp/tmpzpmekavc | unknown | Legacy build_space fixture ACL; removal rejected by automatic approval review: blocked by policy |

Older creator-only ACL fixtures and menu probes remain as listed in [S3 report](../../59-language-neutral-views-s3-report.md).
The 58 top-level ILK files deleted by Claude have not been recreated by this task.

## Unreadable prior fixtures

- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-_ztlfb0p: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-_ztlfb0p'
- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-vpiz1tj5: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-vpiz1tj5'
- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-rt5_3vb7: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-rt5_3vb7'
- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-nrqse2kj: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-nrqse2kj'
- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-n80amp14: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-n80amp14'
- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-fr3kdbgg: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-fr3kdbgg'
- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-akgnxd9z: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-akgnxd9z'
- E:\project\byname\oneui-views\build\capture\viewc-tests\viewc-9qim1tkb: [WinError 5] 拒绝访问。: 'E:\\project\\byname\\oneui-views\\build\\capture\\viewc-tests\\viewc-9qim1tkb'

New compiler and watcher fixtures use automatic cleanup. Formal raw CSV/JSON and two PNGs are requested deliverables, not temporary duplicates.

This run removed five temporary screenshot/CSS experiment files and six superseded replay helper/response files from build/codex-views.
The readable starting total was 24,584,825,374 bytes; the final total above includes both worktrees.
Current replay scripts/response files are retained for review and reuse; no new configuration tree was created.
