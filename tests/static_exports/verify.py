"""Verify public DLL exports survive while a static consumer discards them."""
import importlib.util
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("demand_benchmark", root / "scripts/benchmark-demand.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
consumer, shared = map(Path, sys.argv[1:])
data = consumer.read_bytes()
assert module.exports(data) == 0, "static executable leaked library exports"
assert b"ONEUI_UNUSED_RUNTIME_MARKER" not in data, "unused implementation was retained"
assert module.exports(shared.read_bytes()) == 2, "DLL lost its public exports"
subprocess.run([str(consumer)], check=True)
