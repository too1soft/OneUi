"""Real Android IME window + InputConnection protocol tests, emulator only.

Uses a separately installed keyboard fixture for deterministic native insets;
this does not certify Gboard's Chinese candidate UI. Restores the user's IME.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import time
from android_lab_device import LabDevice

def require(value, message):
    if not value: raise AssertionError(message)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adb', required=True); p.add_argument('--serial', required=True)
    p.add_argument('--no-screenshot', action='store_true', help='Keep the final image from another suite in this batch')
    a = p.parse_args(); device = LabDevice(a.adb, a.serial)
    root = Path(__file__).resolve().parents[1]; out = root/'out/mobile-current'
    fixture = 'net.too1.oneui.inputtests/.TestKeyboard'
    saved_ime = device.shell('settings','get','secure','default_input_method')
    require(saved_ime != fixture, 'Restore the normal IME before running so cleanup is meaningful')
    saved = {k: device.shell('settings','get','system', k) for k in ['font_scale','accelerometer_rotation','user_rotation']}
    report = {'observed_at': datetime.now(timezone.utc).isoformat(), 'checks':[],
              'scope':'Android API36 emulator, ordinary TextField/TextArea; candidate UI pending; selection handles and password have separate suites'}
    def record(name, **data):
        report['checks'].append({'name':name,'result':'pass',**data}); print(name,data,flush=True)
    def wait_status(predicate, why):
        last = None
        for _ in range(60):
            try:
                s = device.status()
                last = s
                if predicate(s): return s
            except AssertionError: pass
            time.sleep(.1)
        raise AssertionError(why+': '+repr(last))
    def keyboard(row):
        # Insets can arrive before the newly selected IME has bound and shown
        # its input view. Wait read-only; never retry an editing action.
        previous = None
        for _ in range(30):
            s = device.status()
            ime = device.shell('dumpsys', 'input_method')
            signature = (s['imeBottom'], s['scale'])
            ready = (('mCurId='+fixture) in ime and 'mInputViewStarted=true' in ime
                     and 'mWindowVisible=true' in ime and 'mInShowWindow=false' in ime
                     and s['imeBottom'] > 300 and s.get('editorFocus0') == 1)
            if ready and signature == previous: break
            previous = signature if ready else None
            time.sleep(.15)
        else: raise AssertionError('Test keyboard binding/presentation did not stabilize: '+repr(s))
        # Fixture owns five 52dp rows; position derives from actual OS IME inset.
        height = int(device.shell('wm','size').split('x')[-1])
        y = height - s['imeBottom'] + (row+.5)*round(52*s['scale'])
        device.shell('input','tap',500,round(y)); time.sleep(.15)
    process = None
    try:
        for key, value in [('accelerometer_rotation','0'),('user_rotation','0'),('font_scale','1.0')]:
            device.shell('settings','put','system',key,value)
        device.adb('install','-r',out/'ime-test/input-tests.apk')
        # Separate instrumentation process exercises the actual bridge object.
        process = subprocess.Popen(device.command+['shell','am','instrument','-w',
            'net.too1.oneui.inputtests/net.too1.oneui.inputtests.InputProtocol'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        wait_status(lambda s:s['pageId']=='layout/basic', 'Instrumentation activity not ready')
        device.area(); device.navigate(2,'features/text'); device.tap('editor0')
        protocol = process.communicate(timeout=35)[0].decode('utf-8','replace'); process = None
        (out/'ime-protocol.log').write_text(protocol,encoding='utf-8')
        require('PASS:' in protocol and 'FAIL:' not in protocol, protocol)
        record('actual-inputconnection-protocol', checks='Chinese composition, emoji, selection, batch, invalid/stale/suspend')
        device.shell('ime','enable',fixture); device.shell('ime','set',fixture)
        device.shell('am','force-stop',device.package)
        device.shell('am','start','-W','-n',device.package+'/.LabActivity')
        initial = device.settled(); device.area(); full = device.bounds
        device.navigate(2,'features/text'); identity = device.status()['page']; device.tap('editor0')
        s = wait_status(lambda s:s['imeBottom']>300,'IME inset not delivered'); device.area()
        require(device.bounds[3] < full[3]-300, 'Surface did not resize for native IME')
        require(s['gpuContexts']==initial['gpuContexts'] and s['gpuResizes']>initial['gpuResizes'],
                'IME resize rebuilt EGL/Ganesh instead of retaining the context')
        keyboard(1); s=wait_status(lambda s:s['editorComposing0']==1, 'Preedit not delivered')
        require(s['editor0'][4]==0, 'preedit changed committed contents')
        keyboard(2); s=wait_status(lambda s:s['editor0'][4]==2 and s['editorComposing0']==0,'Chinese commit missing')
        keyboard(3); s=wait_status(lambda s:s['editor0'][4]==3,'Android wide emoji should be one codepoint')
        keyboard(4); wait_status(lambda s:s['editor0'][4]==2,'emoji delete split a surrogate')
        record('os-keyboard-compose-commit-delete-and-insets', ime_bottom=s['imeBottom'], surface=device.bounds,
               gpu_contexts=s['gpuContexts'], gpu_resizes=s['gpuResizes'])
        before = device.status(); time.sleep(2); after=device.status()
        require(after['frames']-before['frames']<=12, 'Caret repainted at display refresh rate')
        record('caret-does-not-repaint-every-callback', seconds=2, paints=after['frames']-before['frames'],
               callbacks=after['callbacks']-before['callbacks'])
        device.back(); wait_status(lambda s:s['imeBottom']==0, 'Back did not dismiss IME'); device.area()
        require(device.status()['pageId']=='features/text','Back left page before hiding keyboard')
        device.tap('button0'); time.sleep(.2)
        s=device.status(); require(s['editor0'][5:7]==[0,2], 'Select all failed')
        device.shell('input','text','hello'); time.sleep(.2)
        require(device.status()['editor0'][4]==5, 'Hardware keyboard did not replace selection')
        if device.status()['imeBottom']>0:
            device.back(); wait_status(lambda s:s['imeBottom']==0, 'IME hide before undo failed'); device.area()
        device.tap('button1'); device.settled(); require(device.status()['editor0'][4]==4,'Undo failed')
        device.tap('button2'); device.settled(); require(device.status()['editor0'][4]==5,'Redo failed')
        record('back-selection-hardware-undo-redo')
        device.tap('editor1'); wait_status(lambda s:s['imeBottom']>300,'Second editor did not show keyboard')
        device.shell('input','text','line1'); device.shell('input','keyevent','KEYCODE_ENTER')
        device.shell('input','text','line2'); time.sleep(.2)
        require(device.status()['editor1'][4]==11,'Multiline newline failed')
        device.back(); wait_status(lambda s:s['imeBottom']==0,'IME hide failed'); device.area()
        device.navigate(0,'layout/basic'); frame=device.settled()['frames'];time.sleep(1)
        require(device.status()['frames']==frame,'Hidden editor continued repainting')
        device.navigate(2,'features/text'); s=device.settled()
        require(s['page']==identity and s['editor0'][4]==5 and s['editor1'][4]==11,'Page state lost')
        record('multiline-page-retention-hidden-idle')
        device.shell('settings','put','system','font_scale','2.0'); time.sleep(.4)
        device.area() # Wait for Android's configuration/layout pass before touching new geometry.
        s=device.settled(); require(s['fontScale']==2 and s['inputFont']>=28,'SP typography not updated')
        require(s['editor0'][3]>=s['inputFont']*1.5+24-1,'Large type field height clipped')
        device.tap('editor0'); wait_status(lambda s:s['imeBottom']>300,'Large type input unavailable')
        device.shell('input','text','X');time.sleep(.2)
        require(device.status()['editor0'][4]==6,'Large font edit failed')
        device.shell('settings','put','system','user_rotation','1');time.sleep(.5)
        s=device.status(); require(s['page']==identity and s['editor0'][4]==6,'Rotation lost editor contents')
        device.shell('input','text','Y');time.sleep(.2)
        require(device.status()['editor0'][4]==7,'Resized keyboard connection lost focus')
        record('system-large-font-rotation-focus', body_dp=s['inputFont'])
        device.shell('settings','put','system','user_rotation','0');time.sleep(.5)
        device.shell('input','keyevent','KEYCODE_HOME');time.sleep(.3)
        s=device.status(); frame=s['frames'];time.sleep(1)
        require(device.status()['frames']==frame,'Background editor repainted')
        device.shell('am','start','-W','-n',device.package+'/.LabActivity')
        s=wait_status(lambda s:s['lifecycle']==1,'Activity did not resume')
        require(s['editor0'][4]==7,'Background lost contents')
        surface=s['surface']
        device.shell('am','broadcast','--receiver-foreground','-a',device.package+'.DIAGNOSTIC','-p',device.package,'--es','command','rebuild')
        s=wait_status(lambda s:s['lifecycle']==1 and s['surface']>surface,'Surface replacement did not resume')
        require(s['editor0'][4]==7,'Surface replacement lost contents')
        record('background-surface-retains-text')
        device.shell('settings','put','system','font_scale','1.0')
        wait_status(lambda s:s['fontScale']==1,'Default font configuration not delivered')
        if device.stable_area()['imeBottom']>0: device.back();time.sleep(.3)
        wait_status(lambda s:s['imeBottom']==0,'IME dismissal not delivered')
        # Android configuration/inset animations may outlive the font callback.
        # Wait until native footer geometry agrees with the actual Surface.
        require(device.stable_area()['imeBottom']==0,'IME reappeared after dismissal')
        device.tap('source');s=device.settled();require(s['code']==1,'Compiled example not available')
        device.back();device.settled();record('compiled-code-panel')
        if not a.no_screenshot:
            (out/'probe-current.png').write_bytes(device.adb('exec-out','screencap','-p',binary=True))
        report['result']='pass'
    except Exception as error:
        report['result']='fail'; report['failure']=repr(error);raise
    finally:
        if process is not None:
            process.terminate();process.wait(timeout=5)
            device.shell('am','force-stop',device.package)
        device.shell('ime','set',saved_ime)
        device.shell('ime','disable',fixture)
        device.adb('uninstall','net.too1.oneui.inputtests')
        for k,v in saved.items():
            device.shell('settings','delete','system',k) if v=='null' else device.shell('settings','put','system',k,v)
        device.shell('rm','-f','/data/local/tmp/oneui-lab.xml','/data/local/tmp/oneui-mobile-text-tests')
        apk=out/'android-x86_64/oneui-mobile-lab.apk'
        report['apk_sha256']=hashlib.sha256(apk.read_bytes()).hexdigest()
        (out/'android-text-test.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
