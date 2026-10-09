"""Emulator-only checks for the real OneUI workbench Android candidate.

Uses native widget geometry and actual screenshots, not Android stand-in controls.
Keeps one fixed JSON report; image comparisons stay in memory. Restores rotation.
"""
import argparse
from datetime import datetime, timezone
from io import BytesIO
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET
from PIL import Image

PACKAGE = 'net.too1.oneui.lab'
REMOTE_XML = '/data/local/tmp/oneui-lab.xml'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', required=True, type=Path)
    parser.add_argument('--serial', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'emulator-\d+', args.serial):
        parser.error('An explicit emulator serial is required; no physical devices')
    root = Path(__file__).resolve().parents[1]
    output = root / 'out/mobile-current/android-lab-test.json'
    report = {'observed_at': datetime.now(timezone.utc).isoformat(), 'serial': args.serial,
              'scope': 'Android x86_64 shared layout/basic integration; not full mobile parity',
              'checks': []}

    def run(*command, binary=False):
        result = subprocess.check_output([str(args.adb), '-s', args.serial, *map(str, command)], timeout=30)
        return result if binary else result.decode('utf-8', 'replace').strip()

    def shell(*command):
        return run('shell', *command)

    def require(condition, message):
        if not condition: raise AssertionError(message)

    def record(name, **details):
        report['checks'].append({'name': name, 'result': 'pass', **details})
        print(name, json.dumps(details, ensure_ascii=False), flush=True)

    def diagnostic(command):
        return shell('am', 'broadcast', '--receiver-foreground', '-a', PACKAGE + '.DIAGNOSTIC',
                     '-p', PACKAGE, '--es', 'command', command)

    def status():
        response = diagnostic('status')
        require('frames=' in response, 'No fresh shell-permission diagnostic response')
        line = response[response.index('frames='):]
        result = {k: float(v) for k, v in re.findall(r'(frames|events|pending|lifecycle|surface|error|dark|scroll|scale|gridBottom)=([\d.]+)', line)}
        result['page'] = re.search(r'page=(\S+)', line)[1]
        for k, v in re.findall(r'((?:button|slider)\d+|grid|theme)=([\d.,-]+)', line):
            result[k] = list(map(float, v.split(',')))
        result['renderer'] = re.search(r'renderer=(.*?) scale=', line)[1]
        require(result['error'] == 0, 'Native rendering/input error: ' + line)
        return result

    def settle():
        for _ in range(30):
            time.sleep(.1)
            result = status()
            if result['pending'] == 0: return result
        raise AssertionError('Host did not become idle within bounded wait')

    def bounds():
        shell('uiautomator', 'dump', REMOTE_XML)
        tree = ET.fromstring(shell('cat', REMOTE_XML))
        node = tree.find('.//node[@class="android.view.SurfaceView"]')
        require(node is not None, 'OneUI surface is not visible')
        return tuple(map(int, re.findall(r'\d+', node.get('bounds'))))

    def capture():
        return Image.open(BytesIO(run('exec-out', 'screencap', '-p', binary=True))).convert('RGB')

    def point(state, rect, fraction=.5):
        x, y, w, h = rect
        return round(area[0] + (x + w * fraction) * state['scale']), round(area[1] + (y + h / 2) * state['scale'])

    def tap(state, key):
        shell('input', 'tap', *point(state, state[key]))

    def slide(state, key, fraction):
        rect = state[key][1:]
        shell('input', 'swipe', *point(state, rect, .5), *point(state, rect, fraction), '400')

    def pss():
        text = shell('dumpsys', 'meminfo', PACKAGE)
        match = re.search(r'TOTAL PSS:\s+(\d+)', text)
        return int(match[1]) if match else None

    saved = {key: shell('settings', 'get', 'system', key)
             for key in ['accelerometer_rotation', 'user_rotation']}
    pid = ''
    try:
        require(shell('getprop', 'ro.kernel.qemu') == '1', 'Target must be an emulator')
        shell('settings', 'put', 'system', 'accelerometer_rotation', '0')
        shell('settings', 'put', 'system', 'user_rotation', '0')
        shell('am', 'force-stop', PACKAGE)
        shell('am', 'start', '-W', '-n', PACKAGE + '/.LabActivity')
        time.sleep(1)
        pid = shell('pidof', PACKAGE)
        state = settle(); area = bounds()
        require(state['lifecycle'] == 1 and state['frames'] > 0, 'First GPU frame missing')
        report.update(pid=pid, renderer=state['renderer'], api=shell('getprop', 'ro.build.version.sdk'), surface=area)
        apk = root / 'out/mobile-current/android-x86_64/oneui-mobile-lab.apk'
        report['apk_sha256'] = hashlib.sha256(apk.read_bytes()).hexdigest()
        time.sleep(3)
        require(status()['frames'] == state['frames'], 'Idle continues rendering')
        record('idle-stops-frames', frames=state['frames'], seconds=3)
        before = capture()
        tap(state, 'button0'); changed = settle()
        after = capture()
        require(before.tobytes() != after.tobytes(), 'Direction click did not change pixels')
        require(changed['page'] == state['page'], 'Direction click replaced the page')
        record('real-button-changes-layout', events=changed['events'])
        slide(changed, 'slider0', .8); changed = settle()
        require(changed['slider0'][0] > 25 and changed['scroll'] == 0, 'Horizontal slider was intercepted')
        record('slider-horizontal-arbitration', gap=changed['slider0'][0], scroll=changed['scroll'])
        x = (area[0] + area[2]) // 2
        shell('input', 'swipe', x, area[1] + round(350 * changed['scale']),
              x, area[1] + round(160 * changed['scale']), '500')
        changed = settle()
        require(changed['scroll'] > 150, 'Vertical touch did not scroll OneUI content')
        slide(changed, 'slider1', .95); changed = settle()
        require(changed['slider1'][0] == 4, 'Grid column slider did not change')
        require(changed['gridBottom'] <= changed['grid'][1] + changed['grid'][3] + .1 and
                changed['slider1'][2] >= changed['gridBottom'], 'Grid overlaps the following control')
        record('touch-scroll-and-grid', scroll=changed['scroll'], columns=changed['slider1'][0])
        tap(changed, 'theme'); changed = settle()
        require(changed['dark'] == 1, 'Real OneUI theme button failed')
        record('theme-retains-controls', page=changed['page'], gap=changed['slider0'][0], columns=changed['slider1'][0])
        initial_pss = pss()
        retained = changed
        for _ in range(20):
            diagnostic('rebuild'); time.sleep(.3); current = settle()
            require(current['surface'] > changed['surface'], 'Surface generation did not advance')
            for key in ['page', 'dark', 'scroll']:
                require(current[key] == retained[key], 'Surface rebuild lost ' + key)
            for key in ['slider0', 'slider1']:
                require(current[key][0] == retained[key][0], 'Surface rebuild reset parameter')
            changed = current
        final_pss = pss()
        require(initial_pss is not None and final_pss is not None and final_pss - initial_pss < 65536,
                'Surface rebuild memory growth exceeds 64 MiB smoke-test guard')
        record('surface-rebuild-preserves-identity', repeats=20, generation=changed['surface'],
               initial_pss_kib=initial_pss, final_pss_kib=final_pss)
        shell('input', 'keyevent', 'KEYCODE_HOME')
        for _ in range(20):
            time.sleep(.2); background = status()
            if background['lifecycle'] == 3: break
        time.sleep(3)
        require(background['lifecycle'] == 3 and background['pending'] == 0 and
                status()['frames'] == background['frames'], 'Background keeps drawing')
        shell('am', 'start', '-W', '-n', PACKAGE + '/.LabActivity')
        resumed = settle()
        require(resumed['page'] == retained['page'] and resumed['scroll'] == retained['scroll'] and resumed['dark'] == 1,
                'Background/resume lost page state')
        record('background-stops-and-resumes', background_frames=background['frames'], resumed_frames=resumed['frames'])
        shell('settings', 'put', 'system', 'user_rotation', '1'); time.sleep(1)
        wide = settle(); area = bounds()
        require(area[2] - area[0] > area[3] - area[1], 'Landscape geometry invalid')
        require(wide['page'] == retained['page'] and wide['dark'] == 1 and wide['slider1'][0] == 4,
                'Rotation recreated the page model')
        require(wide['theme'][1] >= 0 and wide['theme'][1] + wide['theme'][3] <= (area[3]-area[1])/wide['scale'],
                'Landscape footer is outside the surface')
        record('rotation-retains-model', surface=area, generation=wide['surface'])
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
