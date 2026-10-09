"""Bounded real-touch navigation/lifecycle regression for the shared Android lab.

Uses only an explicitly selected emulator; no diagnostic navigation shortcut.
Writes one current report, restores settings and reuses the existing screenshot.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

PACKAGE = 'net.too1.oneui.lab'
XML = '/data/local/tmp/oneui-lab.xml'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', required=True)
    parser.add_argument('--serial', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'emulator-\d+', args.serial):
        parser.error('Use an explicit emulator serial')
    root = Path(__file__).resolve().parents[1]
    report = {'observed_at': datetime.now(timezone.utc).isoformat(), 'checks': [],
              'scope': 'Four integrated pages; full 27-page mobile parity remains pending'}

    def adb(*parts, binary=False):
        data = subprocess.check_output([args.adb, '-s', args.serial, *map(str, parts)], timeout=30)
        return data if binary else data.decode('utf-8', 'replace').strip()

    def shell(*parts): return adb('shell', *parts)

    def require(condition, message):
        if not condition: raise AssertionError(message)

    def record(name, **data):
        report['checks'].append({'name': name, 'result': 'pass', **data})
        print(name, json.dumps(data, ensure_ascii=False), flush=True)

    def status():
        response = shell('am', 'broadcast', '--receiver-foreground', '-a', PACKAGE + '.DIAGNOSTIC',
                         '-p', PACKAGE, '--es', 'command', 'status')
        require('frames=' in response, 'No fresh debug status')
        line = response[response.index('frames='):]
        fields = '(frames|events|pending|lifecycle|surface|error|dark|scroll|scale|pages|catalog|drawer|wide|code|activePage)'
        s = {k: float(v) for k, v in re.findall(fields + r'=([\d.]+)', line)}
        s['pageId'] = re.search(r'pageId=(\S+)', line)[1]
        s['page'] = re.search(r'page=(\S+)', line)[1]
        for key, values in re.findall(r'((?:button|slider|group)\d+|menu|theme|source|nav:[a-z/]+)=([\d.,-]+)', line):
            s[key] = list(map(float, values.split(',')))
        s['plots'] = {name: {k: float(v) for k, v in re.findall(r'(ticks|running|load|plotTime|meshFallbacks)=([\d.]+)', fields)}
                      for name, fields in re.findall(r'retained\[([^]]+)\]\{([^}]+)\}', line)}
        require(s['error'] == 0, 'Native error: ' + line)
        return s

    def settled():
        for _ in range(35):
            time.sleep(.1); s = status()
            if s['pending'] == 0: return s
        raise AssertionError('Expected page to settle without ongoing frames')

    def area():
        shell('uiautomator', 'dump', XML)
        tree = ET.fromstring(shell('cat', XML))
        node = tree.find('.//node[@class="android.view.SurfaceView"]')
        require(node is not None, 'No visible workbench SurfaceView')
        return tuple(map(int, re.findall(r'\d+', node.get('bounds'))))

    bounds = ()

    def tap(key, state=None):
        s = state or status(); x, y, w, h = s[key]
        px, py = round(bounds[0] + (x + w/2)*s['scale']), round(bounds[1] + (y + h/2)*s['scale'])
        if key.startswith(('nav:', 'group')):
            for _ in range(12):
                top = bounds[1] + round(76*s['scale'])
                bottom = bounds[3] - round(24*s['scale'])
                if top <= py <= bottom: break
                start, end = (bottom, top + 30) if py > bottom else (top + 30, bottom)
                shell('input', 'swipe', px, start, px, end, '350')
                s = settled(); x, y, w, h = s[key]
                px, py = round(bounds[0] + (x+w/2)*s['scale']), round(bounds[1] + (y+h/2)*s['scale'])
        require(bounds[0] <= px < bounds[2] and bounds[1] <= py < bounds[3], 'Offscreen control: ' + key)
        shell('input', 'tap', px, py)

    def navigate(category, page):
        s = status()
        if not s['wide']:
            if not s['drawer']: tap('menu', s)
            settled()
        tap('group' + str(category)); settled()
        tap('nav:' + page)
        time.sleep(.2)
        require(status()['pageId'] == page, 'Navigation failed: ' + page)

    def back(): shell('input', 'keyevent', 'KEYCODE_BACK')

    saved = {key: shell('settings', 'get', 'system', key)
             for key in ['accelerometer_rotation', 'user_rotation']}
    try:
        require(shell('getprop', 'ro.kernel.qemu') == '1', 'Physical devices are excluded')
        shell('settings', 'put', 'system', 'accelerometer_rotation', '0')
        shell('settings', 'put', 'system', 'user_rotation', '0')
        shell('am', 'force-stop', PACKAGE)
        shell('am', 'start', '-W', '-n', PACKAGE + '/.LabActivity')
        state = settled(); bounds = area()
        require(state['catalog'] == 27 and state['pages'] == 1, 'Catalog or lazy construction mismatch')
        basic = state['page']; tap('button0'); settled()
        navigate(2, 'features/controls'); controls = settled()['page']
        tap('button0'); settled()
        navigate(0, 'layout/basic'); state = settled()
        require(state['page'] == basic and state['pages'] == 2, 'Returning recreated the page')
        record('lazy-pages-and-identity', catalog=27, pages=state['pages'])
        tap('source'); state = settled()
        require(state['code'] == 1 and state['activePage'] == 0, 'Code panel did not suspend page')
        back(); require(settled()['code'] == 0, 'System back did not close code')
        tap('menu'); require(settled()['drawer'] == 1, 'Drawer did not open')
        back(); require(settled()['drawer'] == 0, 'System back did not close drawer')
        record('code-drawer-system-back')
        navigate(1, 'performance/particles'); settled()
        tap('button0'); time.sleep(.7); state = status()
        particles = state['page']; ticks = state['plots']['performance/particles']['ticks']
        time.sleep(.6); require(status()['plots']['performance/particles']['ticks'] > ticks, 'Scene did not animate')
        tap('menu'); state = settled(); ticks = state['plots']['performance/particles']['ticks']
        time.sleep(1); require(status()['plots']['performance/particles']['ticks'] == ticks, 'Covered page is still sampling')
        back(); time.sleep(.4)
        require(status()['plots']['performance/particles']['ticks'] > ticks, 'Drawer close did not resume scene')
        record('animation-pauses-under-drawer', ticks=ticks)
        navigate(2, 'features/controls'); state = settled()
        require(state['page'] == controls, 'Control page identity lost')
        ticks = state['plots']['performance/particles']['ticks']; frames = state['frames']
        time.sleep(2); state = status()
        require(state['frames'] == frames and state['plots']['performance/particles']['ticks'] == ticks,
                'Hidden scene or idle controls kept scheduling frames')
        record('hidden-page-and-idle-stop', ticks=ticks, frames=frames)
        navigate(1, 'performance/particles'); time.sleep(.3); state = status()
        require(state['page'] == particles and state['plots']['performance/particles']['ticks'] > ticks,
                'Returning lost scene state')
        shell('input', 'keyevent', 'KEYCODE_HOME'); time.sleep(.5); state = status()
        frames = state['frames']; ticks = state['plots']['performance/particles']['ticks']
        time.sleep(1)
        require(status()['frames'] == frames and status()['plots']['performance/particles']['ticks'] == ticks,
                'Background scene did not stop')
        shell('am', 'start', '-W', '-n', PACKAGE + '/.LabActivity'); time.sleep(.5)
        require(status()['plots']['performance/particles']['ticks'] > ticks, 'Foreground did not resume')
        tap('button0'); settled()
        record('background-foreground-retains-scene', ticks=ticks)
        navigate(1, 'performance/charts'); settled(); tap('button0'); time.sleep(.4)
        require(status()['plots']['performance/charts']['ticks'] > 0, 'Shared chart did not animate')
        tap('button0'); settled()
        record('shared-chart-runs-and-pauses')
        for _ in range(8):
            navigate(2, 'features/controls'); settled()
            navigate(1, 'performance/particles'); settled()
        state = settled(); require(state['pages'] == 4 and state['page'] == particles, 'Repeated navigation grew page cache')
        record('bounded-retained-cache', rounds=8, pages=state['pages'])
        tap('theme'); settled()
        shell('settings', 'put', 'system', 'user_rotation', '1'); time.sleep(1)
        state = settled(); bounds = area()
        require(state['wide'] == 1 and state['dark'] == 1 and state['page'] == particles, 'Expanded layout lost state')
        require('menu' not in state and 'group0' in state, 'Expanded layout needs retained sidebar')
        navigate(2, 'features/controls'); require(settled()['page'] == controls, 'Sidebar navigation failed')
        record('expanded-sidebar-dark-theme', surface=bounds)
        back(); require(settled()['page'] == basic, 'Back did not return to initial page')
        record('back-to-retained-basic')
        apk = root / 'out/mobile-current/android-x86_64/oneui-mobile-lab.apk'
        report.update(result='pass', apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest())
    except Exception as error:
        report.update(result='fail', error=str(error)); raise
    finally:
        for key, value in saved.items():
            shell('settings', 'delete', 'system', key) if value == 'null' else shell('settings', 'put', 'system', key, value)
        shell('rm', '-f', XML)
        (root/'out/mobile-current/android-navigation-test.json').write_text(
            json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__': main()
