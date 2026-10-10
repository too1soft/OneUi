"""Summarize a complete fixed WP4 batch; retain outliers, validate warm counts."""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

SCENES = {
    1: '1→2 分屏', 2: '2→4 分屏', 3: '4→8 分屏', 4: '8→16 分屏',
    5: '16 叶比例变为 0.4', 6: '16 叶均分', 7: '16 叶最大化', 8: '还原',
    9: '关闭第 16 个窗格', 10: '补回第 16 个窗格', 11: 'pane 拆为 tab',
    12: '单叶 tab 合入', 13: '拖动预览 200ms', 14: '释放落点',
    15: 'SFTP 窄形态', 16: 'SFTP 并排/详情', 17: '监控容器切换', 18: '返回终端',
}


def read(path):
    with path.open(encoding='utf-8', newline='') as file:
        return list(csv.DictReader(file))


def quantile(values, percent):
    return sorted(values)[(len(values) - 1) * percent // 100] if values else None


def stats(rows):
    ordinary = [r for r in rows if r['phase'] != '2']
    values = [float(r['work_submit_ms']) for r in ordinary]
    warm = [float(r['work_submit_ms']) for r in rows if r['phase'] == '2']
    # The native count describes the grid's retained layout. In compact tool
    # scenes its DockItem ancestor is hidden; those are not visible terminals.
    sixteen = [float(r['work_submit_ms']) for r in ordinary
               if r['visible'] == '16' and r['scene'] not in {'15', '16', '17'}]
    return {
        'frames': len(values), 'max_ms': max(values, default=0),
        'p50_ms': quantile(values, 50), 'p95_ms': quantile(values, 95),
        'p99_ms': quantile(values, 99), 'slow_frames': sum(v > 16.0 for v in values),
        'warm_frames': len(warm), 'warm_max_ms': max(warm, default=0),
        'all_max_ms': max(values + warm, default=0),
        'visible_16_frames': len(sixteen), 'visible_16_max_ms': max(sixteen, default=0),
        'visible_16_slow_frames': sum(v > 16.0 for v in sixteen),
        'present_p95_ms': quantile([float(r['present_ms']) for r in ordinary], 95),
        'interval_p95_ms': quantile([float(r['interval_ms']) for r in ordinary], 95),
        'interval_max_ms': max((float(r['interval_ms']) for r in ordinary), default=0),
    }


def validate(rows, repeats, path):
    pairs = Counter((int(r['cycle']), int(r['scene'])) for r in rows)
    expected = {(c, s) for c in range(repeats) for s in SCENES}
    if set(pairs) != expected:
        raise ValueError(f'{path}: incomplete scene/cycle set')
    warm = Counter((r['cycle'], r['scene']) for r in rows if r['phase'] == '2')
    if any(count > 1 for count in warm.values()):
        raise ValueError(f'{path}: more than one excluded warm frame in one operation')


def table(title, groups):
    lines = [f'## {title}', '', '| 场景 | 非预热帧 | 最大 ms | P95 ms | >16ms 帧 |',
             '|---|---:|---:|---:|---:|']
    for scene, name in SCENES.items():
        value = groups[str(scene)]
        lines.append(f"| {scene}. {name} | {value['frames']} | {value['max_ms']:.4f} | "
                     f"{value['p95_ms']:.4f} | {value['slow_frames']} |")
    return lines + ['']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    output = args.directory
    rounds, all_rows = {}, []
    for number in range(1, 6):
        path = output / f'gl-round-{number}.csv'
        rows = read(path)
        validate(rows, 100, path)
        rounds[str(number)] = stats(rows)
        all_rows.extend(rows)
    gl = {str(s): stats([r for r in all_rows if int(r['scene']) == s]) for s in SCENES}
    software_rows = read(output / 'software-reference.csv')
    validate(software_rows, 1, 'software')
    software = {str(s): stats([r for r in software_rows if int(r['scene']) == s]) for s in SCENES}
    first, early, ready = {}, [], []
    for number in range(20):
        path = output / f'first-{number:02}.csv'
        rows = read(path)
        if sum(r['phase'] == '2' for r in rows) != 1:
            raise ValueError(f'{path}: expected exactly one first-split warm frame')
        first[path.stem] = stats(rows)
        (early if number < 10 else ready).extend(rows)
    result = {'gl': gl, 'rounds': rounds, 'all_gl': stats(all_rows),
              'software': software, 'first': first,
              'first_early': stats(early), 'first_ready': stats(ready)}
    (output / 'metrics.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
    lines = ['# WP4 实测性能', '', '仅 phase=2 为预热排除；每次操作最多一个。',
             '分位数使用排序后 floor((N-1)×p/100)；不剔除离群值。', '']
    lines += table('NVIDIA OpenGL：5×100 次', gl)
    lines += ['## 首次分屏：20 个新进程', '',
              '| 时机 | 进程 | 最大 ms（非预热） | P95 ms | 慢帧 | 预热最大 ms |',
              '|---|---:|---:|---:|---:|---:|']
    for name, key in [('第 1 帧后请求', 'first_early'), ('第 16 帧后请求', 'first_ready')]:
        value = result[key]
        lines.append(f"| {name} | 10 | {value['max_ms']:.4f} | {value['p95_ms']:.4f} | "
                     f"{value['slow_frames']} | {value['warm_max_ms']:.4f} |")
    lines += [''] + table('软件后端：参考，一次循环', software)
    (output / 'performance.md').write_text('\n'.join(lines), encoding='utf-8')
    print(json.dumps(result['all_gl'], indent=2))


if __name__ == '__main__':
    main()
