"""Compare actual retained trees before accepting paired timing samples."""
import json
import sys
from pathlib import Path

directory = Path(sys.argv[1])
fields = ("tag", "parentId", "depth", "frame", "preferred", "visible", "disabled", "selected", "text", "style",
          "paintFontSize", "paintFontWeight", "checked", "rowHeight", "items")
failures = []
files = sorted(directory.glob("*-ir-*.json"))
if len(files) != 16:
    raise SystemExit(f"Expected 16 initial/warmup pairs, found {len(files)}")
for file in files:
    other = file.with_name(file.name.replace("-ir-", "-manual-"))
    if not other.exists():
        failures.append(f"Missing {other}")
        continue
    left = json.loads(file.read_text(encoding="utf-8"))["nodes"]
    right = json.loads(other.read_text(encoding="utf-8"))["nodes"]
    if len(left) != len(right):
        failures.append(f"{file.name}: node counts {len(left)} != {len(right)}")
        continue
    differences = [(i, key, a.get(key), b.get(key)) for i, (a, b) in enumerate(zip(left, right))
                   for key in fields if a.get(key) != b.get(key)]
    if differences:
        failures.append(f"{file.name}: {len(differences)} differences; first: {str(differences[:4])[:800]}")
    else:
        print(f"PASS {file.name}: {len(left)} native nodes, geometry, typography and state")
if failures:
    print("\n".join(failures))
    sys.exit(1)
