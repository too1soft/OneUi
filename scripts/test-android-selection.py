"""Real Android pointer dispatch to OneUI insertion and selection handles."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import time
from android_lab_device import LabDevice

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adb',required=True);p.add_argument('--serial',required=True)
    a=p.parse_args();d=LabDevice(a.adb,a.serial);out=Path(__file__).resolve().parents[1]/'out/mobile-current'
    saved={k:d.shell('settings','get','system',k) for k in ['font_scale','accelerometer_rotation','user_rotation']}
    ime=d.shell('settings','get','secure','default_input_method');fixture='net.too1.oneui.inputtests/.TestKeyboard'
    report={'observed_at':datetime.now(timezone.utc).isoformat(),'checks':[]};process=None;logs=[]
    def wait(predicate):
        for _ in range(80):
            try:
                s=d.status()
                if predicate(s):return s
            except AssertionError:pass
            time.sleep(.1)
        raise AssertionError('Expected selection test state not reached')
    try:
        for k,v in [('font_scale','1.0'),('accelerometer_rotation','0'),('user_rotation','0')]:d.shell('settings','put','system',k,v)
        d.adb('install','-r',out/'ime-test/input-tests.apk')
        for _ in range(25):
            if fixture in d.shell('ime','list','-a','-s'):break
            time.sleep(.2)
        else:raise AssertionError('Installed keyboard service was not registered')
        d.shell('ime','enable',fixture);d.shell('ime','set',fixture)
        for mode,editor in [('single',0),('multiline',1)]:
            process=subprocess.Popen(d.command+['shell','am','instrument','-w','-e','mode',mode,
                'net.too1.oneui.inputtests/net.too1.oneui.inputtests.SelectionProtocol'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            wait(lambda s:s['pageId']=='layout/basic');d.area();d.navigate(2,'features/text');d.tap('editor'+str(editor))
            # The instrumentation reads the real View location after focus.
            # Do not wait for UIAutomator idle while the editor caret blinks.
            wait(lambda s:s['editorFocus'+str(editor)]==1)
            d.shell('am','broadcast','--receiver-foreground','-a','net.too1.oneui.inputtests.RUN','-p',d.package)
            text=process.communicate(timeout=120)[0].decode('utf-8','replace');process=None;logs.append(mode+'\n'+text)
            (out/'selection-protocol.log').write_text('\n'.join(logs),encoding='utf-8')
            if 'FAIL:' in text or 'PASS: suspend-removes-handles' not in text:raise AssertionError(text)
            for line in text.splitlines():
                if line.startswith('PASS: '):report['checks'].append({'name':mode+':'+line[6:],'result':'pass'})
            print(mode,'passed',flush=True)
        report['result']='pass'
    except Exception as error:
        report['result']='fail';report['failure']=repr(error);raise
    finally:
        if process is not None:
            process.terminate();logs.append(process.communicate(timeout=5)[0].decode('utf-8','replace'))
            (out/'selection-protocol.log').write_text('\n'.join(logs),encoding='utf-8')
            d.shell('am','force-stop',d.package)
        d.shell('ime','set',ime);d.shell('ime','disable',fixture);d.adb('uninstall','net.too1.oneui.inputtests')
        for k,v in saved.items():d.shell('settings','delete','system',k) if v=='null' else d.shell('settings','put','system',k,v)
        d.shell('rm','-f','/data/local/tmp/oneui-lab.xml')
        report['apk_sha256']=hashlib.sha256((out/'android-x86_64/oneui-mobile-lab.apk').read_bytes()).hexdigest()
        (out/'android-selection-test.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
