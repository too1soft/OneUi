#!/usr/bin/env python3
"""Compile the optional VPN SDK through an existing SSH alias; no app installs."""
import argparse
import base64
import io
import json
from pathlib import Path
import shlex
import subprocess
import tarfile

REMOTE = r'''
import datetime,hashlib,io,json,os,pathlib,shutil,subprocess,sys,tarfile
base=pathlib.Path.home()/'.cache/oneui-release-review'
base.mkdir(parents=True,exist_ok=True)
root=base/'vpn-current'
root.mkdir(exist_ok=True)
if root.is_symlink() or root.resolve().parent!=base.resolve():raise RuntimeError('Unexpected output path')
if shutil.disk_usage(root).free<21*1024**3:raise RuntimeError('Insufficient disk headroom')
source=root/'source'
source.mkdir(exist_ok=True)
if source.is_symlink() or source.resolve().parent!=root.resolve():raise RuntimeError('Unexpected source path')
hashes={}
with tarfile.open(fileobj=io.BytesIO(sys.stdin.buffer.read()),mode='r:gz') as archive:
 for item in archive.getmembers():
  target=source/item.name
  if not item.isfile() or target.resolve().is_relative_to(source.resolve()) is False:
   raise RuntimeError('Unexpected archive member')
  data=archive.extractfile(item).read()
  target.parent.mkdir(parents=True,exist_ok=True)
  target.write_bytes(data)
  hashes[item.name]=hashlib.sha256(data).hexdigest()
report={'tested_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_sha256':hashes,'checks':[],'native_vpn_traffic_tested':False}
def run(name,args):
 p=subprocess.run([str(a) for a in args],capture_output=True,text=True,cwd=source,timeout=180)
 report['checks'].append({'name':name,'exit':p.returncode,'output':(p.stdout+p.stderr)[-4000:]})
 if p.returncode:
  (root/'verification.json').write_text(json.dumps(report,indent=2))
  raise RuntimeError(name+': '+(p.stdout+p.stderr)[-4000:])
def sdk(name):return subprocess.check_output(['xcrun','--sdk',name,'--show-sdk-path'],text=True).strip()
include=source/'oneui/include'
cpp=[source/'oneui/src/vpn'/f for f in ['config.cpp','session.cpp','c_api.cpp']]
swift=sorted((source/'oneui/platform/ios/vpn').glob('*.swift'))
mac=root/'macos-arm64'
mac.mkdir(exist_ok=True)
flags=['-std=c++17','-DONEUI_VPN_STATIC','-I'+str(include),'-O2','-target','arm64-apple-macos13.0']
objects=[]
for src in cpp:
 obj=mac/(src.stem+'.o');objects.append(obj)
 run('macOS compile '+src.name,['xcrun','clang++',*flags,'-c',src,'-o',obj])
library=mac/'liboneui_vpn.a'
run('macOS archive',['xcrun','libtool','-static','-o',library,*objects])
run('macOS C++ consumer compile',['xcrun','clang++',*flags,source/'oneui/tests/vpn/session_tests.cpp',library,'-o',mac/'session-tests'])
run('macOS C++ lifecycle tests',[mac/'session-tests'])
run('macOS C consumer compile',['xcrun','clang','-std=c11','-DONEUI_VPN_STATIC','-I'+str(include),'-c',source/'oneui/tests/vpn/c_api_tests.c','-o',mac/'c-tests.o'])
run('macOS C consumer link',['xcrun','clang++',mac/'c-tests.o',library,'-o',mac/'c-tests'])
run('macOS C ABI tests',[mac/'c-tests'])
if (source/'product/mobile/src/connection_model.cpp').is_file():
 run('macOS product SDK consumer compile',['xcrun','clang++',*flags,'-I'+str(source/'product/mobile/include'),source/'product/mobile/src/connection_model.cpp',source/'product/mobile/tests/connection_model_tests.cpp',library,'-o',mac/'product-tests'])
 run('macOS product SDK consumer tests',[mac/'product-tests'])
cache=root/'swift-module-cache'
bridge=source/'native-bridge.h'
bridge.write_text('#include "oneui/bindings/go/oneuivpn/native/native_bridge.h"\n#include "oneui/include/oneui/mobile/native_module.h"\n#include "oneui/vpn/json_api.h"\n#include "oneui/mobile/application_ios.h"\n' + ('#include "product/mobile/core/cmd/mobilecore/mobile_core.h"\n' if (source/'product/mobile/core/cmd/mobilecore/mobile_core.h').is_file() else ''))
swift+=sorted((source/'product/mobile/platform/ios').glob('*.swift'))
for sdk_name,target,folder in [('iphoneos','arm64-apple-ios15.0','ios-arm64'),('iphonesimulator','arm64-apple-ios15.0-simulator','ios-simulator-arm64')]:
 out=root/folder;out.mkdir(exist_ok=True)
 sysroot=sdk(sdk_name)
 objects=[]
 for src in cpp:
  obj=out/(src.stem+'.o');objects.append(obj)
  run(folder+' compile '+src.name,['xcrun','--sdk',sdk_name,'clang++','-std=c++17','-DONEUI_VPN_STATIC','-I'+str(include),'-O2','-target',target,'-isysroot',sysroot,'-c',src,'-o',obj])
 run(folder+' archive',['xcrun','libtool','-static','-o',out/'liboneui_vpn.a',*objects])
 run(folder+' Swift native API typecheck',['xcrun','--sdk',sdk_name,'swiftc','-typecheck','-parse-as-library','-swift-version','5','-target',target,'-sdk',sysroot,'-I'+str(source/'oneui/include'),'-module-cache-path',cache,'-import-objc-header',bridge,*swift])
report['retained_bytes']=sum(p.stat().st_size for p in root.rglob('*') if p.is_file() and not p.is_symlink())
report['checks_passed']=len(report['checks'])
report['scope']='macOS executed contracts and product consumer; iOS device/simulator compilation and Swift typecheck, no installed app or VPN traffic'
(root/'verification.json').write_text(json.dumps(report,indent=2))
# Source staging is a reproducible upload fixture, never a checkout or user data.
if source.resolve().parent==root.resolve() and not source.is_symlink():shutil.rmtree(source)
print(json.dumps(report))
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssh-host', default='oneui-mac')
    parser.add_argument('--product-root', type=Path)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    product = options.product_root
    files = [
        ('oneui', root, 'include/oneui/vpn/c_api.h'),
        ('oneui', root, 'include/oneui/vpn/session.h'),
        ('oneui', root, 'include/oneui/vpn/json_api.h'),
        ('oneui', root, 'include/oneui/mobile/application_ios.h'),
        *[('oneui', root, f'src/vpn/{name}.cpp') for name in ['config', 'session', 'c_api']],
        ('oneui', root, 'tests/vpn/session_tests.cpp'),
        ('oneui', root, 'tests/vpn/c_api_tests.c'),
        ('oneui', root, 'bindings/go/oneuivpn/native/native_bridge.h'),
        ('oneui', root, 'include/oneui/mobile/native_module.h'),
        *[('oneui', root, str(p.relative_to(root)).replace('\\', '/'))
          for p in sorted((root / 'platform/ios/vpn').glob('*.swift'))],
    ]
    if product:
        files += [('product', product, name) for name in [
            'mobile/include/wangyunlian/connection_model.h',
            'mobile/src/connection_model.cpp', 'mobile/tests/connection_model_tests.cpp']]
        files += [('product', product, 'mobile/core/cmd/mobilecore/mobile_core.h')]
        files += [('product', product, str(path.relative_to(product)).replace('\\', '/')) for path in sorted((product / 'mobile/platform/ios').glob('*.swift'))]
    data = io.BytesIO()
    with tarfile.open(fileobj=data, mode='w:gz') as archive:
        for prefix, base, relative in files:
            archive.add(base / relative, arcname=f'{prefix}/{relative}', recursive=False)
    remote = 'import base64;exec(base64.b64decode(' + repr(base64.b64encode(REMOTE.encode()).decode()) + '))'
    result = subprocess.run(['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
                             '-o', 'ConnectTimeout=10', options.ssh_host,
                             '/usr/bin/python3 -c ' + shlex.quote(remote)],
                            input=data.getvalue(), capture_output=True, timeout=600)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors='replace') + result.stdout.decode(errors='replace'))
    report = json.loads(result.stdout)
    output = root / 'out/mobile-current/vpn-host/apple-verification.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'checks_passed': report['checks_passed'], 'scope': report['scope'],
                      'retained_bytes': report['retained_bytes'], 'report': str(output)}))


if __name__ == '__main__':
    main()
