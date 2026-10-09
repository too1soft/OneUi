"""Bounded emulator-only integration checks; reuse the installed test APK.

Screenshots are inspected in memory, never accumulated on disk. Only this test
package is launched. Rotation settings are restored, including on failure.
"""
import argparse
from collections import Counter
from datetime import datetime, timezone
from io import BytesIO
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET
from PIL import Image, ImageChops


PACKAGE = 'net.too1.oneui.mobileprobe'
REMOTE_XML = '/data/local/tmp/oneui-probe.xml'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', required=True, type=Path)
    parser.add_argument('--serial', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'emulator-\d+', args.serial):
        parser.error('Use an explicit emulator serial; physical devices are not supported')
    output = Path(__file__).resolve().parents[1] / 'out/mobile-current/android-test.json'
    report = {'observed_at': datetime.now(timezone.utc).isoformat(), 'serial': args.serial,
              'checks': [], 'scope': 'Runtime/EGL test host, not full OneUI mobile controls'}

    def run(*command, binary=False):
        value = subprocess.check_output([str(args.adb), '-s', args.serial, *command], timeout=30)
        return value if binary else value.decode('utf-8', 'replace').strip()

    def shell(*command):
        return run('shell', *command)

    def require(condition, message):
        if not condition:
            raise AssertionError(message)

    def record(name, **details):
        report['checks'].append({'name': name, 'result': 'pass', **details})
        print(name, json.dumps(details, ensure_ascii=False), flush=True)

    def numbers(text):
        return {k: int(v) for k, v in re.findall(r'(frames|events|cancels|lifecycle|surface|touches|pending|error)=(\d+)', text)}

    def ui():
        shell('uiautomator', 'dump', REMOTE_XML)
        tree = ET.fromstring(shell('cat', REMOTE_XML))
        surface = tree.find('.//node[@class="android.view.SurfaceView"]')
        button = tree.find('.//node[@text="重建绘制表面"]')
        status = next(n.get('text') for n in tree.iter('node') if n.get('text', '').startswith('frames='))
        require(surface is not None and button is not None, 'Probe is not in foreground')
        bounds = lambda n: tuple(map(int, re.findall(r'\d+', n.get('bounds'))))
        return numbers(status), bounds(surface), bounds(button), status

    def logs():
        return run('logcat', '-d', '-t', '500', '--pid=' + pid, 'OneUIMobile:I', 'AndroidRuntime:E', '*:S')

    def square(bounds):
        with Image.open(BytesIO(run('exec-out', 'screencap', '-p', binary=True))) as capture:
            crop = capture.convert('RGB').crop(bounds)
        colors = Counter(crop.getdata())
        greens = [(count, rgb) for rgb, count in colors.items()
                  if rgb[0] < 80 and 80 < rgb[1] < 160 and rgb[2] < 100]
        require(bool(greens), 'No GPU test square in screenshot')
        _, color = max(greens)
        mask = ImageChops.difference(crop, Image.new('RGB', crop.size, color)).convert('L')
        return mask.point(lambda value: 255 if value == 0 else 0).getbbox()

    saved = {k: shell('settings', 'get', 'system', k)
             for k in ['accelerometer_rotation', 'user_rotation']}
    pid = ''
    try:
        require(shell('getprop', 'ro.kernel.qemu') == '1', 'Target must be an emulator')
        shell('settings', 'put', 'system', 'accelerometer_rotation', '0')
        shell('settings', 'put', 'system', 'user_rotation', '0')
        shell('am', 'start', '-W', '-n', PACKAGE + '/.ProbeActivity')
        time.sleep(1)
        pid = shell('pidof', PACKAGE)
        first, bounds, _, status = ui()
        require(first['frames'] > 0 and first['error'] == 0 and first['lifecycle'] == 1, 'Initial draw failed')
        report.update(pid=pid, renderer=status.split('EGL: ')[-1], api=shell('getprop', 'ro.build.version.sdk'))
        time.sleep(3)
        idle = ui()[0]
        require(idle['frames'] == first['frames'] and idle['pending'] == 0, 'Idle redraw loop')
        record('idle-stops-frames', frames=idle['frames'], interval_seconds=3)
        before = square(bounds)
        x1, y1, x2, y2 = bounds
        shell('input', 'swipe', str((x1+x2)//2), str((y1+y2)//2),
              str(x1+(x2-x1)*3//4), str(y1+(y2-y1)*3//4), '600')
        moved, bounds, button, _ = ui()
        position = square(bounds)
        require(moved['events'] > first['events'] and moved['frames'] > first['frames'], 'Touch did not render')
        require(moved['touches'] == 0 and moved['error'] == 0 and position != before, 'Touch release/position failed')
        time.sleep(2)
        require(ui()[0]['frames'] == moved['frames'], 'Touch left a redraw loop')
        record('touch-moves-gpu-square', events=moved['events'], frames=moved['frames'], square=position)
        previous = moved
        for index in range(3):
            shell('input', 'tap', str((button[0]+button[2])//2), str((button[1]+button[3])//2))
            time.sleep(.3)
            current, bounds, button, _ = ui()
            require(current['surface'] > previous['surface'] and current['frames'] > previous['frames'], 'Surface was not recreated')
            require(current['error'] == 0 and square(bounds) == position, 'Surface recreation lost position')
            previous = current
        record('surface-rebuild-retains-state', repeats=3, generation=current['surface'], square=position)
        shell('input', 'keyevent', 'KEYCODE_HOME')
        time.sleep(2)
        background = numbers(logs())
        require(background['lifecycle'] == 3 and background['pending'] == 0, 'Background lifecycle incorrect')
        time.sleep(3)
        require(numbers(logs())['frames'] == background['frames'], 'Background keeps drawing')
        shell('am', 'start', '-W', '-n', PACKAGE + '/.ProbeActivity')
        resumed, bounds, _, _ = ui()
        require(shell('pidof', PACKAGE) == pid and resumed['lifecycle'] == 1, 'Runtime process not retained')
        require(resumed['frames'] > background['frames'] and square(bounds) == position and resumed['error'] == 0, 'Resume lost state/draw')
        record('background-and-resume', background_frames=background['frames'], resumed_frames=resumed['frames'])
        shell('settings', 'put', 'system', 'user_rotation', '1')
        time.sleep(1)
        landscape, wide, _, _ = ui()
        require(wide[2]-wide[0] > wide[3]-wide[1] > 0, 'Landscape surface has invalid geometry')
        require(landscape['surface'] > resumed['surface'] and landscape['error'] == 0, 'Rotation failed')
        square(wide)
        record('rotation-recreates-surface', generation=landscape['surface'], bounds=wide)
        require('FATAL EXCEPTION' not in logs(), 'Android runtime crash')
        report['result'] = 'pass'
    except Exception as error:
        report.update(result='fail', error=str(error))
        raise
    finally:
        for key, value in saved.items():
            shell('settings', 'delete', 'system', key) if value == 'null' else shell('settings', 'put', 'system', key, value)
        shell('rm', '-f', REMOTE_XML)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
