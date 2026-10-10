"""Exclusive native probes, separate from the uninstrumented paired benchmark."""
import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

directory = Path(sys.argv[1])
output = {}
for phase in ("before", "after"):
    operations = defaultdict(list)
    pending_paint = defaultdict(dict)
    for row in csv.DictReader((directory / f"profile-{phase}.csv").open(encoding="utf-8")):
        case, op, stage = row["case"], row["operation"], row["stage"]
        if not case.startswith("ir-") or op not in ("flush", "paint"):
            continue
        if op == "paint":
            pending_paint[case][stage] = int(row["exclusive_ns"])
            if stage == "paint":
                operations[(case, op)].append(pending_paint.pop(case))
            continue
        if stage == op or not operations[(case, op)]:
            operations[(case, op)].append({})
        operations[(case, op)][-1][stage] = int(row["exclusive_ns"])
    cases = {}
    for case in ("update1000", "insert_remove10", "move10"):
        # One initial scheduled flush/frame, then 50 warmup samples per case.
        skip = 101 if case == "insert_remove10" else 51
        flush = operations[("ir-" + case, "flush")][skip:]
        # Paint emits layout first, then the outer paint probe.
        paint = operations[("ir-" + case, "paint")]
        paint = paint[skip:]
        assert len(flush) == (400 if case == "insert_remove10" else 200), (phase, case, len(flush))
        stages = {}
        for stage in ("delta", "vm", "conversion", "rows", "keyed", "flush"):
            stages[stage] = statistics.mean(r.get(stage, 0) for r in flush) / 1e6
        for stage in ("layout", "paint"):
            stages[stage] = statistics.mean(r.get(stage, 0) for r in paint) / 1e6
        cases[case] = {"samples": len(flush), "paint_samples": len(paint), "mean_exclusive_ms": stages}
    output[phase] = cases
(directory / "profile-summary.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
print(json.dumps(output, indent=2))
