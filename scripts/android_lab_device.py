"""Small real-input driver for the current Android lab, never the user's phone."""
import re
import subprocess
import time
import xml.etree.ElementTree as ET

def surface_bounds(shell, path='/data/local/tmp/oneui-lab.xml'):
    # Window creation/configuration can temporarily leave UiAutomation without
    # an active root. Never consume an older dump as if it described this frame.
    for _ in range(3):
        shell('rm', '-f', path)
        reply = shell('uiautomator', 'dump', path)
        if 'dumped to:' in reply:
            tree = ET.fromstring(shell('cat', path))
            node = tree.find('.//node[@class="android.view.SurfaceView"]')
            if node is not None:
                return tuple(map(int, re.findall(r'\d+', node.get('bounds'))))
        time.sleep(.3)
    raise AssertionError('No fresh SurfaceView after three UI hierarchy reads')

class LabDevice:
    package = 'net.too1.oneui.lab'
    def __init__(self, adb, serial):
        if not re.fullmatch(r'emulator-\d+', serial):
            raise ValueError('An explicit emulator serial is required')
        self.command = [str(adb), '-s', serial]
        self.bounds = (0, 0, 0, 0)
    def adb(self, *parts, binary=False):
        output = subprocess.check_output(self.command + list(map(str, parts)), timeout=30)
        return output if binary else output.decode('utf-8', 'replace').strip()
    def shell(self, *parts): return self.adb('shell', *parts)
    def status(self):
        raw = self.shell('am', 'broadcast', '--receiver-foreground', '-a', self.package+'.DIAGNOSTIC',
                         '-p', self.package, '--es', 'command', 'status')
        if 'frames=' not in raw: raise AssertionError(raw)
        line = raw[raw.index('frames='):]
        result = {k: float(v) for k, v in re.findall(r'\b(\w+)=(-?\d+(?:\.\d+)?)(?=[ \"}]|$)', line)}
        for k, v in re.findall(r'([\w:/]+)=(-?[\d.,-]+,[-\d.,]+)', line):
            result[k] = list(map(float, v.split(',')))
        result['pageId'] = re.search(r'pageId=(\S+)', line)[1]
        result['page'] = re.search(r'page=(\S+)', line)[1]
        if result['error'] != 0: raise AssertionError(line)
        return result
    def settled(self):
        for _ in range(40):
            time.sleep(.1)
            state = self.status()
            if state['pending'] == 0 and state['lifecycle'] == 1: return state
        raise AssertionError('Page did not stop drawing')
    def area(self):
        self.bounds = surface_bounds(self.shell)
        return self.bounds
    def stable_area(self):
        previous = None
        stable = 0
        for _ in range(15):
            self.area(); state = self.status()
            footer = state.get('source')
            matches = footer and abs((footer[1]+footer[3]+8)*state['scale']-
                                    (self.bounds[3]-self.bounds[1])) < 3
            signature = (self.bounds, state['imeBottom'], state['scale'])
            stable = stable + 1 if matches and state['lifecycle'] == 1 and signature == previous else 0
            if stable >= 2: return state
            previous = signature
            time.sleep(.2)
        raise AssertionError('Native footer and visible Surface did not stabilize')
    def tap(self, key, scroll=False):
        for attempt in range(15):
            state = self.status()
            for _ in range(40):
                if state['lifecycle'] == 1: break
                time.sleep(.1); state = self.status()
            if state['lifecycle'] != 1: raise AssertionError('Cannot tap an inactive host')
            x, y, w, h = state[key][:4]
            scale = state['scale']; left, top, right, bottom = self.bounds
            px, py = round(left+(x+w/2)*scale), round(top+(y+h/2)*scale)
            if not scroll or top+76*scale < py < bottom-72*scale: break
            a, b = (bottom-76*scale, top+96*scale) if py >= bottom-72*scale else (top+96*scale, bottom-76*scale)
            self.shell('input', 'swipe', px, round(a), px, round(b), '300')
            self.settled()
        if not (left <= px < right and top <= py < bottom): raise AssertionError('Offscreen '+key)
        self.shell('input', 'tap', px, py)
    def navigate(self, category, page):
        s = self.status()
        if not s['wide'] and not s['drawer']: self.tap('menu'); self.settled()
        self.tap('group'+str(category), True); self.settled()
        self.tap('nav:'+page, True); self.settled()
        if self.status()['pageId'] != page: raise AssertionError('Failed navigation '+page)
    def back(self): self.shell('input', 'keyevent', 'KEYCODE_BACK')
