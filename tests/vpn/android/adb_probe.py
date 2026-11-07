"""Bounded emulator-only power/network probes with exact original-state restore."""
import json
import subprocess
import time


class EmulatorProbe:
    def __init__(self, adb, serial):
        if not serial.startswith('emulator-'):
            raise RuntimeError('Stability perturbations require an explicit emulator')
        self.adb = [str(adb), '-s', serial]
        self.original = {}

    def shell(self, *args):
        result = subprocess.run(self.adb + ['shell', *map(str, args)], capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=15)
        if result.returncode:
            raise RuntimeError('Named emulator stability command failed')
        return result.stdout.strip()

    def health(self, package):
        try:
            raw = self.shell('run-as', package, '--user', '0', 'cat', 'cache/mobile-vpn-acceptance-health')
            return json.loads(raw)
        except (RuntimeError, ValueError):
            return None

    def screen_off(self):
        self.shell('input', 'keyevent', '223')
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            if 'mWakefulness=Asleep' in self.shell('dumpsys', 'power'): return
            time.sleep(.2)
        raise RuntimeError('Emulator did not confirm screen-off state')

    def screen_on(self):
        self.shell('input', 'keyevent', '224')

    def offline(self):
        for setting in ['wifi_on', 'mobile_data']:
            value = self.shell('settings', 'get', 'global', setting)
            if value not in {'0', '1'}:
                raise RuntimeError('Unknown original network state; no perturbation applied')
            self.original[setting] = value
        self.shell('svc', 'wifi', 'disable')
        self.shell('svc', 'data', 'disable')

    def restore(self):
        for setting, command in [('wifi_on', 'wifi'), ('mobile_data', 'data')]:
            if setting in self.original:
                self.shell('svc', command, 'enable' if self.original[setting] == '1' else 'disable')
        self.original.clear()
        self.screen_on()

    def wait_underlays(self, package, present, seconds=20):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            value = self.health(package)
            if value is not None and (value['underlays'] > 0) == present:
                return value
            time.sleep(.5)
        raise RuntimeError('Emulator underlay transition was not acknowledged')
