"""Compare the old full-size shadow path with optimized native captures (Pillow)."""
import sys
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
root = Path(sys.argv[1])
for backend, extension in [("cpu", "png"), ("gpu", "ppm")]:
    for scale in [1., 1.25, 1.5]:
        name = f"{backend}-{scale:.6f}.{extension}"
        before = Image.open(root / "reference" / name).convert("RGB")
        after = Image.open(root / "optimized" / name).convert("RGB")
        assert before.size == after.size
        delta = ImageChops.difference(before, after)
        maximum = max(v[1] for v in delta.getextrema())
        mean = sum(ImageStat.Stat(delta).mean) / 3
        # Raster must be exact. Ganesh lattice sampling at fractional scale can
        # differ at a few edge pixels; bound both worst channel and total error.
        assert maximum <= (0 if backend == "cpu" else 12), (name, maximum)
        assert mean <= (0 if backend == "cpu" else .005), (name, mean)
        print(f"{name}: max={maximum}, mean={mean:.6f}")
