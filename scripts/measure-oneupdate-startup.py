"""Measure a real installer until its existing UI-ready log marker.

No installation is started. Close the displayed window normally after READY.
Windows process times and memory come from the launched process handle.
"""
import argparse
import ctypes
from ctypes import wintypes
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import time


class Memory(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage", "QuotaPagedPoolUsage",
            "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage", "PagefileUsage",
            "PeakPagefileUsage", "PrivateUsage")]


def filetime(value):
    return (value.dwHighDateTime << 32 | value.dwLowDateTime) / 10_000_000


def process_metrics(process):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    times = [wintypes.FILETIME() for _ in range(4)]
    get_times = kernel.GetProcessTimes
    get_times.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    get_times.restype = wintypes.BOOL
    memory = Memory()
    memory.cb = ctypes.sizeof(memory)
    get_memory = psapi.GetProcessMemoryInfo
    get_memory.argtypes = [wintypes.HANDLE, ctypes.POINTER(Memory), wintypes.DWORD]
    get_memory.restype = wintypes.BOOL
    handle = wintypes.HANDLE(int(process._handle))
    if not get_times(handle, *(ctypes.byref(item) for item in times)) or \
            not get_memory(handle, ctypes.byref(memory), memory.cb):
        raise ctypes.WinError(ctypes.get_last_error())
    return {"created_epoch": filetime(times[0]) - 11_644_473_600,
            "cpu_ms": (filetime(times[2]) + filetime(times[3])) * 1000,
            "working_set_bytes": memory.WorkingSetSize,
            "peak_working_set_bytes": memory.PeakWorkingSetSize,
            "private_bytes": memory.PrivateUsage, "peak_private_bytes": memory.PeakPagefileUsage}


def events(path):
    result = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"\[(\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)(\.\d+)? .*?\].*? event=(\w+)", line)
        if match:
            year, month, day, hour, minute, second = map(int, match.groups()[:6])
            stamp = datetime.datetime(year, month, day, hour, minute, second,
                                      tzinfo=datetime.timezone.utc).timestamp()
            result[match[8]] = stamp + float(match[7] or 0)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--install-root", type=Path, required=True,
                        help="Unused isolated path; installation is never started")
    parser.add_argument("--timeout", type=float, default=45)
    parser.add_argument("--terminate-after-ready", action="store_true",
                        help="Stop this QA process after sampling; no UI interaction is measured")
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("Windows process metrics required")
    if args.output.exists():
        raise ValueError("Refusing to overwrite a measurement")
    binary = args.binary.resolve(strict=True)
    if args.install_root.exists():
        raise ValueError("Startup comparisons require an unused install root")
    roots = [Path(os.environ["LOCALAPPDATA"]) / "OneUpdate/installer-logs",
             Path(os.environ["TEMP"]) / "OneUpdate/installer-logs"]
    existing = {path for root in roots for path in root.glob("installer-*.log")}
    process = subprocess.Popen([str(binary), "--install-root", str(args.install_root.resolve())])
    started = time.monotonic()
    log = None
    while time.monotonic() - started < args.timeout:
        if process.poll() is not None:
            raise RuntimeError(f"Installer exited before UI ready: {process.returncode}")
        if log is None:
            for root in roots:
                # Windows can reuse a PID while its previous diagnostic log
                # still exists. Never accept a log from before this launch.
                matches = sorted(path for path in root.glob(f"installer-*-{process.pid}.log")
                                 if path not in existing)
                if matches:
                    log = matches[-1]
                    break
        timeline = events(log) if log else {}
        if "ui_window_visible" in timeline:
            metrics = process_metrics(process)
            ready_ms = (timeline["ui_window_visible"] - metrics.pop("created_epoch")) * 1000
            if not 0 <= ready_ms <= args.timeout * 1000:
                raise ValueError("UI-ready timestamp does not belong to this process launch")
            result = {"binary": str(binary), "pid": process.pid, "log": str(log),
                      "ready_ms": ready_ms,
                      "shell_ms": (timeline["ui_window_visible"] - timeline["ui_shell_starting"]) * 1000,
                      "font_and_window_ms": (timeline["ui_window_created"] - timeline["ui_shell_starting"]) * 1000,
                      **metrics}
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            print("READY " + json.dumps(result), flush=True)
            if args.terminate_after_ready:
                process.terminate()
                process.wait(timeout=10)
                # Only this measurement's diagnostic marker is temporary.
                marker = log.with_suffix(".active")
                if marker.resolve().parent != log.resolve().parent or not marker.stem.endswith(f"-{process.pid}"):
                    raise ValueError("Unexpected measurement marker path")
                marker.unlink(missing_ok=True)
                result["terminated_after_ready"] = True
            # Otherwise the reviewer closes the window using normal controls.
            process.wait(timeout=120)
            result["exit_code"] = process.returncode
            args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            return
        time.sleep(0.01)
    raise TimeoutError(f"UI-ready marker missing; pid={process.pid}, log={log}")


if __name__ == "__main__":
    main()
