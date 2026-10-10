"""Bounded, sequential GPU acceptance. Run the native-feature example first.
No OS input, capture polling, cache reconfiguration, or unbounded evidence.
"""
from pathlib import Path
import argparse, csv, json, os, subprocess, statistics, time, struct, zlib

parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--rounds', type=int, default=5)
parser.add_argument('--repeats', type=int, default=100)
parser.add_argument('--skip-captures', action='store_true')
args = parser.parse_args()
exe = args.exe.resolve()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env.pop('ONEUI_WORKSPACE_TRACE', None)
env.pop('ONEUI_RENDERER', None)

def run(name, options, software=False, timeout=330):
    settings = dict(env)
    if software:
        settings['ONEUI_RENDERER'] = 'software'
    result = subprocess.run([str(exe), *options], env=settings,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=timeout)
    log = result.stdout.decode('utf-8', errors='replace')
    (out / (name + '.log')).write_text(log, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{name}: exit {result.returncode}\n{log}')
    print(f'{name}: PASS', flush=True)

run('identity', ['--acceptance', '--output', str(out / 'identity-frames.csv')])
if not args.skip_captures:
    # Reproducible local static PNG input, no file picker or user data involved.
    def chunk(kind, data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    pixels=b''.join(b'\0'+b''.join(bytes((32+x*2,64+y*2,100+(x+y)//2,255)) for x in range(64)) for y in range(64))
    fixture=out/'preview-fixture.png'
    fixture.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',64,64,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b''))
    shots = [
        ('light-wide', ['--panes', '4']),
        ('dark-wide', ['--panes', '4', '--dark']),
        ('light-narrow', ['--panes', '2', '--narrow']),
        ('dark-narrow', ['--panes', '2', '--narrow', '--dark']),
        ('floating', ['--panes', '4', '--floating']),
        ('sftp-wide', ['--tool', 'sftp', '--height', '800']),
        ('sftp-medium', ['--tool', 'sftp', '--width', '950', '--height', '600']),
        ('sftp-narrow', ['--tool', 'sftp', '--narrow']),
        ('monitor-wide', ['--tool', 'monitor', '--width', '1800']),
        ('themes', ['--settings', '--gallery', '--dark']),
        ('media', ['--settings', '--dark', '--media', str(fixture)]),
        ('drag-preview', ['--panes', '4', '--drag-preview']),
    ]
    for name, options in shots:
        run(name, [*options, '--capture', str(out / (name + '.png'))])
for i in range(20):
    name = f'first-{i:02}'
    run(name, ['--first-split', '--terminal-only',
               *(['--early'] if i < 10 else []), '--output', str(out / (name + '.csv'))])
for i in range(args.rounds):
    name = f'gl-round-{i+1}'
    run(name, ['--perf', '--terminal-only', '--repeats', str(args.repeats),
               '--output', str(out / (name + '.csv'))], timeout=600)
run('software-reference', ['--perf', '--terminal-only', '--repeats', '1',
                          '--output', str(out / 'software-reference.csv')], software=True)

def summarize(paths):
    groups = {}
    for path in paths:
        with path.open(newline='', encoding='utf-8') as file:
            for row in csv.DictReader(file):
                if row['phase'] == '2':
                    continue
                groups.setdefault(row['scene'], []).append(float(row['work_submit_ms']))
    return {scene: {'frames': len(v), 'max_ms': max(v), 'p95_ms': sorted(v)[(len(v)-1)*95//100],
                    'slow_frames': sum(x > 16 for x in v)} for scene, v in groups.items()}
summary = {'gl': summarize(out.glob('gl-round-*.csv')),
           'software': summarize([out / 'software-reference.csv']),
           'first_split': {p.stem: summarize([p]) for p in out.glob('first-*.csv')}}
(out / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary['gl'], indent=2), flush=True)
