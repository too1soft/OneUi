"""The packaged SDK must use static text objects and retain archive names."""
import json
import sys
from pathlib import Path

recipe = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
text = [Path(path) for path in recipe["archives"] if Path(path).name == "oneui_text.lib"]
assert len(text) == 1 and text[0].parent.name == "static", text
assert all(Path(path).is_file() for path in recipe["archives"])
assert all(not name.startswith("oneui_") for name in recipe["systemLibraries"])
