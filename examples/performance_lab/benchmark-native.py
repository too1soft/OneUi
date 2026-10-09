#!/usr/bin/env python3
"""Bounded native benchmark: one reusable case directory and baseline/candidate summaries."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--phase', choices=['baseline', 'candidate'], required=True)
    parser.add_argument('--rounds', type=int, default=2)
    parser.add_argument('--seconds', type=int, default=5)
    parser.add_argument('--min-frame-hz', type=float, default=0,
                        help='Reject animated samples below an explicit acceptance threshold')
    parser.add_argument('--scenes', nargs='+', choices=['idle', 'table', 'chart', 'particles'],
                        default=['idle', 'table', 'chart', 'particles'])
    parser.add_argument('--loads', nargs='+', choices=['medium', 'heavy'], default=['medium', 'heavy'])
    args = parser.parse_args()
    if not 1 <= args.rounds <= 5 or not 1 <= args.seconds <= 30:
        parser.error('Use 1..5 rounds and 1..30 sample seconds')
    args.output.mkdir(parents=True, exist_ok=True)
    work = args.output / 'current-case'
    work.mkdir(exist_ok=True)
    metadata = dict(platform=platform.platform(), logical_cpus=os.cpu_count(),
                    seconds=args.seconds, warmup_seconds=3, rounds=args.rounds,
                    scenes=args.scenes, loads=args.loads, width=1050, height=720,
                    cpu_units='percent of all logical CPUs; not weighted for core speed',
                    timing='CPU wall-time; frame_callback_hz is not a GPU/display timestamp',
                    hashes={str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in [args.exe, args.library]})
    (args.output / (args.phase + '-environment.json')).write_text(json.dumps(metadata, indent=2))
    rows = []
    for round_no in range(1, args.rounds + 1):
        cases = [(load, scene) for load in args.loads for scene in args.scenes]
        if round_no % 2 == 0:
            cases.reverse()
        for load, scene in cases:
            report = work / 'renderer-performance.txt'
            report.unlink(missing_ok=True)
            with (work / 'runtime.log').open('w') as log:
                subprocess.run([str(args.exe), '--renderer', 'gpu', '--renderer-benchmark', scene,
                                '--sample-seconds', str(args.seconds), '--load', load,
                                '--width', '1050', '--height', '720', '--output', str(work)],
                               stdout=log, stderr=subprocess.STDOUT, check=True,
                               timeout=args.seconds + 30)
            row = dict(round=round_no, load=load)
            row.update(line.split('=', 1) for line in report.read_text().splitlines() if '=' in line)
            expected = 'metal' if platform.system() == 'Darwin' else 'opengl'
            if row['backend'] != expected or row['backend_changes'] != '0':
                raise RuntimeError('Backend fallback/change: ' + repr(row))
            if row['width'] != '1050' or row['height'] != '720':
                raise RuntimeError('Unexpected window geometry: ' + repr(row))
            if scene != 'idle' and int(row['paint_count']) == 0:
                raise RuntimeError('Animated benchmark did not paint')
            if args.phase == 'candidate' and scene == 'idle' and int(row['paint_count']) > 2:
                raise RuntimeError('Idle page is repainting continuously')
            if scene != 'idle' and float(row['frame_callback_hz']) < args.min_frame_hz:
                raise RuntimeError('Frame cadence regressed: ' + repr(row))
            rows.append(row)
            with (args.output / (args.phase + '.csv')).open('w', newline='') as output:
                writer = csv.DictWriter(output, fieldnames=list(row))
                writer.writeheader()
                writer.writerows(rows)
            print(f"{args.phase} {round_no} {load} {scene}: "
                  f"cpu={row['cpu_percent']}% paint={row['paint_mean_ms']}ms "
                  f"callback={row.get('frame_callback_hz', 'unavailable')}Hz", flush=True)
    # Keep only aggregate evidence after success. On failure current-case is the one reproducer.
    for name in ['runtime.log', 'renderer-performance.txt', 'content-paint.csv']:
        (work / name).unlink(missing_ok=True)
    work.rmdir()


if __name__ == '__main__':
    main()
