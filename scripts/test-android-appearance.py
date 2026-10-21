"""Actual Android system-theme/bar checks. Emulator only; restores OS settings.

Screenshot comparisons stay in memory. One fixed final capture and JSON report.
No debug setters: UI changes use touch; system theme uses Android's uimode API.
"""
import argparse
from collections import Counter
from datetime import datetime, timezone
from io import BytesIO
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time
from android_lab_device import surface_bounds
from PIL import Image

PACKAGE = 'net.too1.oneui.lab'
XML = '/data/local/tmp/oneui-lab.xml'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', required=True)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--no-screenshot', action='store_true', help='Preserve another final capture; pixel checks still run')
    args = parser.parse_args()
    if not re.fullmatch(r'emulator-\d+', args.serial): parser.error('Explicit emulator required')
    root = Path(__file__).resolve().parents[1]
    report = {'observed_at': datetime.now(timezone.utc).isoformat(), 'checks': [],
              'scope': 'Appearance SDK Android native adapter; not complete mobile parity'}

    def adb(*parts, binary=False):
        data = subprocess.check_output([args.adb, '-s', args.serial, *map(str, parts)], timeout=30)
        return data if binary else data.decode('utf-8', 'replace').strip()
    def shell(*parts): return adb('shell', *parts)
    def require(ok, message):
        if not ok: raise AssertionError(message)
    def record(name, **details):
        report['checks'].append({'name': name, 'result': 'pass', **details})
        print(name, json.dumps(details), flush=True)
    def diagnostic(command):
        return shell('am', 'broadcast', '--receiver-foreground', '-a', PACKAGE + '.DIAGNOSTIC',
                     '-p', PACKAGE, '--es', 'command', command)
    def status():
        line = diagnostic('status')
        require('frames=' in line, 'No fresh workbench status')
        fields = 'frames|pending|lifecycle|error|dark|systemTheme|themeMode|appearanceRevision|appliedRevision|statusArgb|navigationArgb|barLightIcons|wide|drawer|scale|surface|code|pages'
        s = {k: float(v) for k, v in re.findall(r'(' + fields + r')=([\d.]+)', line)}
        for key, values in re.findall(r'(button\d+|group\d+|menu|theme|source|safe|nav:[a-z/]+)=([\d.,-]+)', line):
            s[key] = list(map(float, values.split(',')))
        s['page'] = re.search(r'page=(\S+)', line)[1]
        s['pageId'] = re.search(r'pageId=(\S+)', line)[1]
        require(s['error'] == 0, line)
        return s
    def settled():
        for _ in range(40):
            time.sleep(.1); s = status()
            if s['pending'] == 0 and s['lifecycle'] == 1 and s['appearanceRevision'] == s['appliedRevision']: return s
        raise AssertionError('Appearance did not settle/apply: ' + str(s))
    def bounds():
        return surface_bounds(shell, XML)

    area = ()
    def tap(key):
        for _ in range(14):
            s = settled(); x, y, w, h = s[key]
            px = round(area[0] + (x + w/2)*s['scale'])
            py = round(area[1] + (y + h/2)*s['scale'])
            if key in ('menu', 'theme', 'source'): break
            top = area[1] + round(82*s['scale'])
            bottom = area[3] - round(90*s['scale'])
            if top <= py <= bottom: break
            start, end = (bottom, top+30) if py > bottom else (top+30, bottom)
            shell('input', 'swipe', px, start, px, end, '300')
        require(area[0] <= px < area[2] and area[1] <= py < area[3], 'Offscreen: '+key)
        shell('input', 'tap', px, py); settled()
    def open_theme():
        s = settled()
        if not s['wide'] and not s['drawer']: tap('menu')
        tap('group2'); tap('nav:features/theme')
        require(settled()['pageId'] == 'features/theme', 'Theme navigation')
    def night(value):
        nonlocal area
        shell('cmd', 'uimode', 'night', value)
        # Android configuration delivery is asynchronous. Explicit light mode
        # keeps dark=0 across both events, so that alone is not acknowledgement.
        expected = 2 if value == 'yes' else 1
        for _ in range(40):
            s = settled()
            if s['systemTheme'] == expected:
                # The configuration callback precedes WindowManager's input
                # transition. Read a fresh idle root before the next touch.
                area = bounds()
                return settled()
            time.sleep(.1)
        raise AssertionError('System theme observation did not acknowledge '+value)
    def pixels(status_rgb, nav_rgb):
        # Native properties are acknowledged before SurfaceFlinger presents the
        # frame. Check the actual pixels within a bounded presentation window.
        details = []
        for attempt in range(8):
            png = adb('exec-out', 'screencap', '-p', binary=True)
            image = Image.open(BytesIO(png)).convert('RGB')
            s = status(); left, top, right, bottom = map(int, s['safe'])
            w, h = image.size
            require(top > 0 and bottom > 0, 'Expected visible system bars')
            details = []
            for color, box in [(status_rgb, (0, 0, w, top)), (nav_rgb, (0, h-bottom, w, h))]:
                samples = list(image.crop(box).getdata())
                matched = sum(max(abs(a-b) for a, b in zip(pixel, color)) <= 2 for pixel in samples)
                details.append((color, matched / len(samples), Counter(samples).most_common(1)))
            if all(row[1] > .6 for row in details):
                require(area == (left, top, w-right, h-bottom), 'Safe area was lost or applied twice')
                return png
            if attempt < 7: time.sleep(.25)
        raise AssertionError('Native bar pixels did not present: ' + str(details))

    require(shell('getprop', 'ro.kernel.qemu') == '1', 'Do not change physical devices')
    saved = {key: shell('settings', 'get', 'system', key) for key in ['accelerometer_rotation', 'user_rotation']}
    old_night = re.search(r'Night mode: (\w+)', shell('cmd', 'uimode', 'night'))
    require(old_night is not None, 'Cannot restore night mode safely')
    try:
        shell('settings', 'put', 'system', 'accelerometer_rotation', '0')
        shell('settings', 'put', 'system', 'user_rotation', '0')
        shell('cmd', 'uimode', 'night', 'no')
        shell('am', 'force-stop', PACKAGE); shell('am', 'start', '-W', '-n', PACKAGE+'/.LabActivity')
        s = settled(); area = bounds(); basic = s['page']
        require(s['systemTheme'] == 1 and s['themeMode'] == 0 and s['dark'] == 0, 'Initial system theme')
        pixels((245, 247, 244), (245, 247, 244))
        s = night('yes')
        require(s['systemTheme'] == 2 and s['dark'] == 1 and s['page'] == basic, 'System theme recreated/lost page')
        pixels((23, 30, 33), (23, 30, 33))
        require(s['barLightIcons'] == 3, 'Dark bars need light icons')
        record('system-light-dark-live-and-safe-area', safe=s['safe'])
        open_theme(); appearance = settled()['page']
        tap('button1'); s = settled()
        require(s['themeMode'] == 1 and s['dark'] == 0 and s['systemTheme'] == 2, 'Light override masks system query')
        night('no'); s = night('yes')
        require(s['themeMode'] == 1 and s['dark'] == 0, 'System change ignored explicit override')
        tap('button0')
        for _ in range(40):
            s = settled()
            if s['themeMode'] == 0 and s['dark'] == 1: break
            time.sleep(.1)
        else: raise AssertionError('Follow did not restore current system theme: '+str(s))
        record('explicit-mode-and-return-to-system')
        tap('button4'); s = settled()
        pixels((40, 97, 61), (245, 247, 244))
        require(s['barLightIcons'] == 1, 'Per-bar contrast should differ')
        tap('button5'); s = settled()
        pixels((245, 247, 244), (23, 30, 33))
        require(s['barLightIcons'] == 2, 'Opposite per-bar contrast')
        record('independent-bar-colors-icons-and-pixels')
        shell('settings', 'put', 'system', 'user_rotation', '1'); time.sleep(.7)
        s = settled(); area = bounds()
        require(s['page'] == appearance and s['systemTheme'] == 2 and s['wide'] == 1, 'Rotation lost page/config')
        pixels((245, 247, 244), (23, 30, 33))
        record('rotation-retains-style-and-insets', safe=s['safe'])
        shell('settings', 'put', 'system', 'user_rotation', '0'); time.sleep(.5)
        settled(); area = bounds()
        before = settled()['surface']; diagnostic('rebuild'); time.sleep(.5); s = settled()
        require(s['surface'] > before and s['page'] == appearance, 'Surface rebuild lost state')
        pixels((245, 247, 244), (23, 30, 33))
        record('surface-rebuild-retains-appearance')
        shell('input', 'keyevent', 'KEYCODE_HOME'); time.sleep(.3)
        frames = status()['frames']; shell('cmd', 'uimode', 'night', 'no'); time.sleep(.4)
        require(status()['frames'] == frames, 'Background appearance change renders frames')
        shell('am', 'start', '-W', '-n', PACKAGE+'/.LabActivity'); s = settled(); area = bounds()
        require(s['page'] == appearance and s['systemTheme'] == 1 and s['dark'] == 0, 'Resume lost system observation')
        record('background-theme-change-and-resume')
        tap('button3'); pixels((245, 247, 244), (245, 247, 244))
        frames = settled()['frames']; time.sleep(1.2)
        require(status()['frames'] == frames, 'Idle appearance continues repainting')
        record('restore-default-and-idle-stop', frames=frames)
        tap('source'); require(settled()['code'] == 1, 'Compiled source panel')
        shell('input', 'keyevent', 'KEYCODE_BACK'); require(settled()['code'] == 0, 'Back closes source')
        record('compiled-source-and-system-back')
        # Leave an ordinary default screen with actual system settings restored.
        shell('cmd', 'uimode', 'night', old_night[1]); settled()
        # Show all controls near the top for the single fixed visual evidence.
        shell('input', 'swipe', area[2]//2, area[1]+300, area[2]//2, area[3]-250, '300'); settled()
        if not args.no_screenshot:
            (root/'out/mobile-current/probe-current.png').write_bytes(adb('exec-out', 'screencap', '-p', binary=True))
        apk = root/'out/mobile-current/android-x86_64/oneui-mobile-lab.apk'
        report.update(result='pass', apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest())
    except Exception as error:
        report.update(result='fail', error=str(error)); raise
    finally:
        shell('cmd', 'uimode', 'night', old_night[1])
        for key, value in saved.items():
            shell('settings', 'delete', 'system', key) if value == 'null' else shell('settings', 'put', 'system', key, value)
        shell('rm', '-f', XML)
        (root/'out/mobile-current/android-appearance-test.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__': main()
