"""Aggregate paired rounds; keep failures, cold starts and raw samples separate."""
import csv
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path

directory = Path(sys.argv[1])
samples = defaultdict(list)
for file in sorted(directory.glob("*-samples.csv")):
    for row in csv.DictReader(file.open(encoding="utf-8")):
        samples[(row["case"], int(row["round"]), row["path"])].append(int(row["elapsed_ns"]))


def p95(values):
    return sorted(values)[math.ceil(len(values) * .95) - 1]


results = []
assert {key[0] for key in samples} == {
    "ui_event", "dispatcher", "scalar", "text", "themes12",
    "update1000", "insert10", "remove10", "move10",
}, "Missing or unexpected benchmark cases"
for case in sorted({key[0] for key in samples}):
    rounds = sorted({key[1] for key in samples if key[0] == case})
    assert rounds == list(range(5)), (case, rounds)
    metrics = defaultdict(list)
    for round_index in rounds:
        a, b = [samples[(case, round_index, path)] for path in ("ir", "manual")]
        assert len(a) == len(b) == 1000, (case, round_index, len(a), len(b))
        for name, function in (("median", statistics.median), ("p95", p95)):
            metrics[f"ir_{name}_ms"].append(function(a) / 1e6)
            metrics[f"manual_{name}_ms"].append(function(b) / 1e6)
            metrics[f"{name}_delta_percent"].append((function(a) / function(b) - 1) * 100)
    aggregate = {key: statistics.median(values) for key, values in metrics.items()}
    aggregate["case"] = case
    aggregate["pass"] = max(aggregate["median_delta_percent"], aggregate["p95_delta_percent"]) <= 10
    aggregate["paired_rounds"] = dict(metrics)
    results.append(aggregate)
output = directory.parent
(output / "summary.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
lines = ["| 用例 | 手写 median / P95 ms | IR median / P95 ms | median / P95 偏差 | ≤+10% |",
         "|---|---:|---:|---:|---|"]
for r in results:
    lines.append(f"| {r['case']} | {r['manual_median_ms']:.3f} / {r['manual_p95_ms']:.3f} "
                 f"| {r['ir_median_ms']:.3f} / {r['ir_p95_ms']:.3f} "
                 f"| {r['median_delta_percent']:+.1f}% / {r['p95_delta_percent']:+.1f}% "
                 f"| {'通过' if r['pass'] else '未达标'} |")
(output / "results.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
print("\n".join(lines))
