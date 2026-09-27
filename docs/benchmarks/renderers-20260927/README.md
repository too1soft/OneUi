# CPU / GPU renderer baseline — 2026-09-27

See [method, results and limitations](../../41-renderer-selection-and-product-boundaries.md).

- `environment.json`: machine, driver, configuration and executable/DLL SHA256.
- `summary.csv`: all 24 completed runs, three rounds × four scenes × two modes.
- `<round>-<scene>-<mode>/renderer-performance.txt`: renderer identity and aggregate counters.
- `<round>-<scene>-<mode>/content-paint.csv`: CPU wall-time samples for the root widget's content traversal; idle files contain only a header.

All samples use the same clean Release build based on `2d4c168` plus the renderer change. No backend transitions occurred during sampling. Times are not GPU execution time or display FPS. Memory is a process snapshot at the end of each run, not peak memory or VRAM. The CPU percentage is normalized over 32 logical processors. Chart/particle paints are not capped to the same rate; compare paint counts alongside CPU use. Do not add content/submit/blit to total paint time.
