"""Compare identical native workloads with alternating process order."""
import argparse
import ctypes
import hashlib
import importlib.util
import json
import statistics
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--pairs", type=int, default=21)
    parser.add_argument("--cpu", type=int)
    args = parser.parse_args()
    priority = 0
    if args.cpu is not None:
        kernel = ctypes.windll.kernel32
        kernel.GetCurrentProcess.restype = ctypes.c_void_p
        if not kernel.SetProcessAffinityMask(ctypes.c_void_p(kernel.GetCurrentProcess()),
                                            ctypes.c_size_t(1 << args.cpu)):
            raise OSError("Cannot set requested CPU affinity")
        priority = 0x80  # HIGH_PRIORITY_CLASS, inherited affinity; never realtime.
    spec = importlib.util.spec_from_file_location("benchmark_demand",
        Path(__file__).with_name("benchmark-demand.py"))
    benchmark = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(benchmark)
    args.output.mkdir(parents=True, exist_ok=True)
    samples = {"before": [], "after": []}
    binaries = {"before": args.before.resolve(), "after": args.after.resolve()}
    for pair in range(args.pairs):
        for label in (["before", "after"] if pair % 2 == 0 else ["after", "before"]):
            pixels = (args.output / (label + ".bgra")).resolve()
            result = subprocess.run([str(binaries[label]), str(pixels)], check=True,
                capture_output=True, text=True, encoding="utf-8", creationflags=priority)
            samples[label].append(json.loads(result.stdout))
    versions = {}
    for label, executable in binaries.items():
        data = executable.read_bytes()
        pixels = (args.output / (label + ".bgra")).read_bytes()
        versions[label] = {
            "executable_bytes": len(data), "named_exports": benchmark.exports(data),
            "executable_sha256": hashlib.sha256(data).hexdigest(),
            "pixels_sha256": hashlib.sha256(pixels).hexdigest(),
            "median": {key: statistics.median(row[key] for row in samples[label])
                       for key in samples[label][0]},
            "samples": samples[label],
        }
    before, after = versions["before"], versions["after"]
    report = {
        "pairs": args.pairs, "order": "alternating reversed pairs", "cpu": args.cpu,
        "priority": "high" if priority else "normal", "render": "Skia CPU raster 640x400 BGRA",
        "startup_definition": "process-local first view mount; excludes process creation and first paint",
        "versions": versions,
        "pixels_equal": before["pixels_sha256"] == after["pixels_sha256"],
        "size_reduction_percent": 100 * (1 - after["executable_bytes"] / before["executable_bytes"]),
        "metric_change_percent": {key: 100 * (after["median"][key] / value - 1)
                                  for key, value in before["median"].items()},
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    assert report["pixels_equal"], "rendered pixels differ"
    assert after["named_exports"] == 0, "static executable leaked DLL exports"
    print(json.dumps({key: value for key, value in report.items() if key != "versions"}))


if __name__ == "__main__":
    main()
