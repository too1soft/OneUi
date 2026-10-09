#!/usr/bin/env python3
"""Drive only this test's OneUI windows through their native message queue.

Checks UI command wiring/lifecycle, not hardware pointer routing or screen DPI.
Outputs go to the supplied fixed QA directory; no binary copies are created.
"""
import argparse
import ctypes
from ctypes import wintypes as w
import json
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    user.EnumWindows.argtypes = [callback_type, w.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [w.HWND, ctypes.POINTER(w.DWORD)]
    user.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, ctypes.c_int]
    user.GetClientRect.argtypes = [w.HWND, ctypes.POINTER(w.RECT)]
    user.IsWindow.argtypes = user.IsWindowVisible.argtypes = user.IsWindowEnabled.argtypes = [w.HWND]
    user.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
    user.SetProcessDPIAware()
    process = None

    def windows():
        result = {}

        @callback_type
        def visit(hwnd, _):
            pid = w.DWORD()
            user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == process.pid and user.IsWindowVisible(hwnd):
                title = ctypes.create_unicode_buffer(512)
                user.GetWindowTextW(hwnd, title, len(title))
                result[title.value] = hwnd
            return True

        user.EnumWindows(visit, 0)
        return result

    def until(predicate, message):
        deadline = time.monotonic() + 6
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            if process.poll() is not None:
                raise RuntimeError('Demo exited unexpectedly: ' + message)
            time.sleep(.05)
        raise RuntimeError(message)

    def click(hwnd, x, y, logical_width):
        rect = w.RECT()
        assert user.GetClientRect(hwnd, ctypes.byref(rect))
        scale = rect.right / logical_width
        point = (round(y * scale) << 16) | round(x * scale)
        for message, buttons in [(0x200, 0), (0x201, 1), (0x202, 0)]:
            assert user.PostMessageW(hwnd, message, buttons, point)
        time.sleep(.15)

    with (args.output / 'runtime.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(args.exe.resolve()), '--page', 'features/multiwindow',
                                    '--width', '760', '--height', '900', '--output', str(args.output.resolve())],
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            main_window = until(lambda: windows().get('OneUI 试验台 · 多窗口与跨窗拖放'),
                                'Main page missing')
            time.sleep(.3)
            click(main_window, 320, 426, 760)
            child = until(lambda: windows().get('OneUI · 观察窗口'), 'Observer action did not open a window')
            user.PostMessageW(child, 0x10, 0, 0)
            time.sleep(.25)
            assert user.IsWindow(child), 'Close guard did not defer the close request'
            click(child, 250, 286, 540)
            assert user.IsWindow(child), 'Cancel destroyed the observer'
            user.PostMessageW(child, 0x10, 0, 0)
            time.sleep(.2)
            click(child, 250, 234, 540)
            until(lambda: not user.IsWindow(child), 'Accept did not close the observer')
            # Dispatch to the retained closed window must be a no-op.
            click(main_window, 320, 538, 760)
            assert process.poll() is None, 'Dispatch after close failed'
            click(main_window, 320, 594, 760)
            modal = until(lambda: windows().get('OneUI · 异步父子模态'), 'Modal action failed')
            assert not user.IsWindowEnabled(main_window), 'Modal did not restrict parent input'
            click(modal, 220, 128, 500)
            until(lambda: not user.IsWindow(modal), 'Modal confirmation did not close the child')
            until(lambda: user.IsWindowEnabled(main_window), 'Parent input was not restored')
            # Exit preflight must preserve ALL windows when any observer rejects.
            click(main_window, 320, 426, 760)
            child = until(lambda: windows().get('OneUI · 观察窗口'), 'Second observer missing')
            click(main_window, 320, 752, 760)
            time.sleep(.2)
            assert user.IsWindow(main_window), 'Main window closed before all confirmations'
            click(child, 250, 286, 540)
            assert user.IsWindow(main_window) and user.IsWindow(child), 'Cancelled quit closed a window'
            click(main_window, 320, 752, 760)
            time.sleep(.2)
            click(child, 250, 234, 540)
            exit_code = process.wait(timeout=6)
            assert exit_code == 0, f'Confirmed quit returned {exit_code:#x}'
            report = dict(close_deferred=True, close_cancel=True, close_accept=True,
                          closed_dispatch=True, modal_input_restriction=True, modal_input_restored=True,
                          quit_cancel_preserves_windows=True, quit_accept=True,
                          input='Win32 posted client messages; not a physical pointer/DPI acceptance test')
            (args.output / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
            print(json.dumps(report))
        finally:
            if process.poll() is None:
                # This is the uniquely spawned test process, never an existing user window.
                process.terminate()
                process.wait(timeout=5)


if __name__ == '__main__':
    main()
