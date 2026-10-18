"""Real password InputConnection and floating edit menu checks; emulator only."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time
from android_lab_device import LabDevice


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', required=True)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--no-screenshot', action='store_true', help='Preserve another final capture')
    args = parser.parse_args()
    d = LabDevice(args.adb, args.serial)
    out = Path(__file__).resolve().parents[1] / 'out/mobile-current'
    report = {'observed_at': datetime.now(timezone.utc).isoformat(), 'checks': []}
    saved = {k: d.shell('settings', 'get', 'system', k)
             for k in ['font_scale', 'accelerometer_rotation', 'user_rotation']}
    process = None
    saved_ime = d.shell('settings', 'get', 'secure', 'default_input_method')
    fixture = 'net.too1.oneui.inputtests/.TestKeyboard'

    def require(value, message):
        if not value:
            raise AssertionError(message)

    def record(name):
        report['checks'].append({'name': name, 'result': 'pass'})
        print(name, flush=True)

    def wait(predicate):
        last = None
        for _ in range(70):
            try:
                state = d.status()
                last = state
                if predicate(state):
                    return state
            except AssertionError:
                pass
            time.sleep(.1)
        raise AssertionError('Expected device state not reached: '+repr(last))

    def hide_ime():
        if d.stable_area()['imeBottom'] > 0:
            d.back()
            wait(lambda s: s['imeBottom'] == 0)
        require(d.stable_area()['imeBottom'] == 0, 'IME reappeared after dismissal')

    def location(key):
        # Configuration can outlive Activity startup; use the current visible
        # Surface only once the native footer agrees with its physical size.
        d.stable_area()
        previous = None
        stable = 0
        for _ in range(12):
            s = wait(lambda s:s['lifecycle']==1)
            x, y, w, h = s[key][:4]
            left, top, right, bottom = d.bounds
            scale = s['scale']
            px, py = round(left + (x + w / 2) * scale), round(top + (y + h / 2) * scale)
            if top + 80 * scale < py < bottom - 80 * scale:
                # Scroll/reveal/layout updates can move the editor after the
                # footer has stabilized. Confirm the target itself before one
                # gesture; a stale intermediate frame can be hundreds of px off.
                signature = (tuple(s[key][:4]), s['scroll'], s['imeBottom'], d.bounds)
                stable = stable + 1 if signature == previous else 0
                if stable >= 2: return px, py
                previous = signature
                time.sleep(.2)
                continue
            previous = None
            stable = 0
            start, end = (bottom - 90 * scale, top + 120 * scale) if py >= bottom - 80 * scale else (top + 120 * scale, bottom - 90 * scale)
            # Use the ScrollView's padding, not a text selection gesture.
            drag_x = round(left + 12 * scale)
            d.shell('input', 'swipe', drag_x, round(start), drag_x, round(end), 350)
            time.sleep(.4)  # A focused editor may continue its caret clock.
            if not any(d.status().get('editorFocus'+str(i)) for i in range(3)): d.settled()
        raise AssertionError('Cannot reveal ' + key)

    def menu(key):
        hide_ime()
        px, py = location(key)
        # Match the real-input selection suite: event delivery on the emulator
        # can lag shell submission; keep the press held past dispatch timeout.
        d.shell('input', 'swipe', px, py, px, py, 1500)
        time.sleep(.4)

    def items():
        raw = d.shell('am', 'broadcast', '--receiver-foreground', '-a', 'net.too1.oneui.inputtests.MENU',
                      '-p', d.package, '--es', 'command', 'inspect')
        match = re.search(r'data="(.*)"', raw)
        require(match is not None, 'Menu inspector not available')
        return {node['text']: node for node in json.loads(match[1])}

    def action(label):
        nodes = {}
        for _ in range(12):
            nodes = items()
            if label in nodes:
                break
            time.sleep(.2)  # FloatingToolbar relayout/animation is asynchronous.
        require(label in nodes, 'Native edit action missing: ' + label + '; visible=' + repr(nodes))
        x1, y1, x2, y2 = nodes[label]['bounds']
        d.shell('input', 'tap', (x1 + x2) // 2, (y1 + y2) // 2)
        time.sleep(.2)

    try:
        for k, v in [('font_scale', '1.0'), ('accelerometer_rotation', '0'), ('user_rotation', '0')]:
            d.shell('settings', 'put', 'system', k, v)
        d.adb('install', '-r', out / 'ime-test/input-tests.apk')
        process = subprocess.Popen(d.command + ['shell', 'am', 'instrument', '-w', '-e', 'mode', 'secure',
            'net.too1.oneui.inputtests/net.too1.oneui.inputtests.InputProtocol'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        wait(lambda s: s['pageId'] == 'layout/basic')
        def protocol_area():
            # Read the actual View on its UI thread. A second UiAutomation
            # hierarchy client can lose its root while instrumentation starts.
            for _ in range(30):
                raw = d.shell('am', 'broadcast', '--receiver-foreground', '-a',
                              'net.too1.oneui.inputtests.RUN', '-p', d.package, '--es', 'command', 'surface')
                match = re.search(r'data="(\[[\d, -]+\])"', raw)
                if match and d.status()['lifecycle'] == 1:
                    d.bounds = tuple(json.loads(match[1]))
                    return d.bounds
                time.sleep(.2)
            raise AssertionError('Protocol fixture has no visible editor Surface')
        d.area = protocol_area
        d.area()
        d.navigate(2, 'features/text')
        px, py = location('editor2')
        report['initial_password_target'] = {'point': [px, py], 'surface': d.bounds, 'state': d.status()}
        d.shell('input', 'tap', px, py)
        wait(lambda s:s.get('editorFocus2')==1)
        d.shell('am', 'broadcast', '--receiver-foreground', '-a', 'net.too1.oneui.inputtests.RUN', '-p', d.package)
        protocol = process.communicate(timeout=35)[0].decode('utf-8', 'replace')
        process = None
        (out / 'secure-protocol.log').write_text(protocol, encoding='utf-8')
        require('PASS:' in protocol and 'FAIL:' not in protocol, protocol)
        record('password-protocol-privacy-unicode-clipboard-stale')
        d.shell('ime', 'enable', fixture); d.shell('ime', 'set', fixture)
        process = subprocess.Popen(d.command + ['shell', 'am', 'instrument', '-w',
            'net.too1.oneui.inputtests/net.too1.oneui.inputtests.MenuInspector'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        wait(lambda s: s['pageId'] == 'layout/basic')
        def inspected_area():
            for _ in range(20):
                surface = items().get('__surface__')
                if surface and d.status()['lifecycle'] == 1:
                    d.bounds = tuple(surface['bounds'])
                    return d.bounds
                time.sleep(.2)  # Rotation temporarily removes the visible root.
            raise AssertionError('No active SurfaceView after window transition')
        d.area = inspected_area  # Only one UiAutomation service may own the device.
        d.settled(); d.area(); d.navigate(2, 'features/text'); d.tap('editor0')
        wait(lambda s: s.get('editorFocus0') == 1 and s['imeBottom'] > 300)
        d.stable_area()  # Do not inject typing into the focus/IME resize transition.
        d.shell('input', 'text', 'demo')
        wait(lambda s: s['editor0'][4] == 4)
        menu('editor0'); action('select all')
        require(d.status()['editor0'][5:7] == [0, 4], 'Menu selection did not reach C++')
        action('copy')
        record('native-long-press-select-all-copy')
        menu('editor2')
        names = items()
        require('copy' not in names and 'cut' not in names, 'Password menu exposes copy/cut')
        action('paste')
        require(d.status()['editor2'][4] == 4, 'Clipboard paste did not reach password control')
        menu('editor2'); action('select all')
        require(d.status()['editor2'][5:7] == [0, 4], 'Password menu selection failed')
        names = items()
        require('copy' not in names and 'cut' not in names, 'Password selection exposes copy/cut')
        d.back()
        require(d.status()['pageId'] == 'features/text', 'Back left page before closing edit menu')
        record('password-native-menu-paste-no-copy-cut')
        hide_ime()
        menu('editor0'); action('select all'); action('cut')
        require(d.status()['editor0'][4] == 0, 'Native cut did not change control')
        menu('editor0'); action('paste')
        require(d.status()['editor0'][4] == 4, 'Native paste did not restore text')
        record('ordinary-native-menu-cut-paste')
        hide_ime()
        px, py = location('editor2'); d.shell('input', 'tap', px, py)
        wait(lambda s: s.get('editorFocus2') == 1 and s['imeBottom'] > 300)
        d.stable_area()
        d.shell('input', 'text', 'X'); time.sleep(.2)
        require(d.status()['editor2'][4] in (1, 5), 'Password hardware input failed')
        size = d.status()['editor2'][4]; identity = d.status()['page']
        d.shell('settings', 'put', 'system', 'user_rotation', '1'); time.sleep(.6)
        d.stable_area()
        require(d.status()['page'] == identity and d.status()['editor2'][4] == size, 'Rotation lost password')
        d.shell('input', 'text', 'Y'); time.sleep(.2)
        require(d.status()['editor2'][4] == size + 1, 'Password connection lost after rotation')
        d.shell('input', 'keyevent', 'KEYCODE_HOME'); time.sleep(.3)
        frame = d.status()['frames']; time.sleep(.7)
        require(d.status()['frames'] == frame, 'Background editor kept painting')
        d.shell('am', 'start', '-W', '-n', d.package + '/.LabActivity')
        s = wait(lambda s: s['lifecycle'] == 1)
        require(s['editor2'][4] == size + 1, 'Background lost password')
        surface = s['surface']
        d.shell('am', 'broadcast', '--receiver-foreground', '-a', d.package + '.DIAGNOSTIC', '-p', d.package,
                '--es', 'command', 'rebuild')
        s = wait(lambda s: s['lifecycle'] == 1 and s['surface'] > surface)
        require(s['editor2'][4] == size + 1, 'Surface replacement lost password')
        record('password-rotation-background-surface-retention')
        d.shell('settings', 'put', 'system', 'user_rotation', '0'); time.sleep(.5)
        hide_ime()
        px, py = location('editor2')
        if not args.no_screenshot:
            (out / 'probe-current.png').write_bytes(d.adb('exec-out', 'screencap', '-p', binary=True))
        # Clear synthetic password through the real public UI.
        px, py = location('button4')
        report['clear_before'] = {'bounds': d.bounds, 'point': [px, py], 'state': d.status()}
        d.shell('input', 'tap', px, py)
        report['clear_after'] = wait(lambda s: s['editor2'][4] == 0)
        record('clear-password-ui')
        d.shell('am', 'broadcast', '--receiver-foreground', '-a', 'net.too1.oneui.inputtests.MENU',
                '-p', d.package, '--es', 'command', 'quit')
        menu_result = process.communicate(timeout=10)[0].decode('utf-8', 'replace'); process = None
        (out / 'menu-protocol.log').write_text(menu_result, encoding='utf-8')
        require('PASS:' in menu_result and 'FAIL:' not in menu_result, menu_result)
        report['result'] = 'pass'
    except Exception as error:
        report['result'] = 'fail'; report['failure'] = repr(error)
        raise
    finally:
        try:
            if process is not None:
                process.terminate()
                protocol = process.communicate(timeout=5)[0].decode('utf-8', 'replace')
                (out / 'secure-protocol.log').write_text(protocol, encoding='utf-8')
                d.shell('am', 'force-stop', d.package)
            d.shell('ime', 'set', saved_ime); d.shell('ime', 'disable', fixture)
            d.adb('uninstall', 'net.too1.oneui.inputtests')
            for k, v in saved.items():
                d.shell('settings', 'delete', 'system', k) if v == 'null' else d.shell('settings', 'put', 'system', k, v)
            d.shell('rm', '-f', '/data/local/tmp/oneui-lab.xml')
        except Exception as cleanup_error:
            report['cleanup_failure'] = repr(cleanup_error)
            report['result'] = 'fail'
            raise
        finally:
            report['apk_sha256'] = hashlib.sha256((out / 'android-x86_64/oneui-mobile-lab.apk').read_bytes()).hexdigest()
            (out / 'android-secure-test.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
