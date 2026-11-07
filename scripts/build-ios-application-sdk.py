#!/usr/bin/env python3
"""Build a fixed UIKit widget SDK on the existing authorized Mac SSH host.

Reuses its reviewed Skia checkout; never copies a desktop Skia archive into iOS.
The caller supplies freshly audited local managed bytes for the 40 GiB exception.
"""
import argparse
import base64
import io
import json
from pathlib import Path
import shlex
import subprocess
import tarfile

REMOTE = r"""
import datetime,hashlib,io,json,os,pathlib,shutil,subprocess,sys,tarfile,time
home=pathlib.Path.home(); managed=home/'.cache/oneui-release-review'
root=managed/('ios-'+SLICE);root.mkdir(parents=True,exist_ok=True)
if root.is_symlink() or root.resolve().parent!=managed.resolve():raise RuntimeError('Unexpected SDK root')
source=root/'source';source.mkdir(exist_ok=True)
if source.is_symlink() or source.resolve().parent!=root.resolve():raise RuntimeError('Unexpected source fixture')
(source/'.oneui-ios-fixture').write_text('regenerable build snapshot')
manifest=root/'source-files.json';previous=json.loads(manifest.read_text()) if manifest.is_file() else {};current={}
with tarfile.open(fileobj=io.BytesIO(sys.stdin.buffer.read()),mode='r:gz') as archive:
 for item in archive.getmembers():
  path=source/item.name
  if not item.isfile() or not path.resolve().is_relative_to(source.resolve()):raise RuntimeError('Invalid source archive')
  content=archive.extractfile(item).read();digest=hashlib.sha256(content).hexdigest()
  prior=previous.get(item.name,{});stamp=prior.get('mtime',time.time()) if prior.get('sha256')==digest else time.time()
  path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(content);os.utime(path,(stamp,stamp));current[item.name]={'sha256':digest,'mtime':stamp}
manifest.write_text(json.dumps(current))
env=dict(os.environ,PATH=str(managed/'tools/bin')+':/usr/bin:/bin:/usr/sbin:/sbin')
skia=home/'projects/oneui/third_party/skia'
if subprocess.check_output(['git','-C',str(skia),'rev-parse','HEAD'],text=True).strip()!='1f26101197bff9fcd939a791beb3094297436d59':raise RuntimeError('Skia revision mismatch')
log=root/'current.log';checks=[]
def audit():
 total=sum(p.stat().st_size for p in managed.rglob('*') if p.is_file() and not p.is_symlink())
 if LOCAL_BYTES+total>40*1024**3:raise RuntimeError('Cross-machine OneUI artifact budget exceeded')
 if shutil.disk_usage(root).free<21*1024**3:raise RuntimeError('Mac disk headroom below limit')
 return total
def run(name,args,cwd=source):
 audit()
 with log.open('w') as output:
  process=subprocess.Popen([str(a) for a in args],cwd=cwd,env=env,stdout=output,stderr=subprocess.STDOUT)
  last=time.monotonic();count=0
  try:
   while process.poll() is None:
    time.sleep(1)
    files=sum(1 for p in root.rglob('*') if p.is_file())
    if files-count>=100 or time.monotonic()-last>=60:
     audit();count=files;last=time.monotonic()
  except BaseException:
   process.terminate();process.wait(timeout=15);raise
 code=process.returncode;tail=log.read_text(errors='replace')[-6000:]
 checks.append({'name':name,'exit':code,'output':tail})
 (root/'verification.json').write_text(json.dumps({'checks':checks},indent=2))
 if code:raise RuntimeError(name+': '+tail)
 audit()
run('ICU pin and reviewed Unicode patch',['cmake','-DONEUI_ICU_ROOT='+str(skia/'third_party/externals/icu'),'-P',source/'scripts/apply-skia-patches.cmake'])
out=root/'skia';out.mkdir(exist_ok=True)
sim=SLICE=='simulator-arm64';sysroot='iphonesimulator' if sim else 'iphoneos'
args='''is_debug=false
is_official_build=true
target_os="ios"
target_cpu="arm64"
ios_min_target="15.0"
ios_use_simulator=SIM
skia_ios_use_signing=false
skia_enable_ganesh=false
skia_enable_graphite=false
skia_use_metal=false
skia_use_gl=false
skia_use_vulkan=false
skia_use_dawn=false
skia_enable_pdf=false
skia_enable_skottie=false
skia_enable_svg=false
skia_enable_tools=false
skia_use_dng_sdk=false
skia_use_wuffs=false
skia_use_piex=false
skia_use_perfetto=false
skia_use_system_expat=false
skia_use_system_harfbuzz=false
skia_use_system_icu=false
skia_use_system_freetype2=false
skia_use_system_libjpeg_turbo=false
skia_use_system_libpng=false
skia_use_system_libwebp=false
skia_use_system_zlib=false
skia_use_icu=true
skia_use_harfbuzz=true
skia_use_freetype=true
skia_enable_skparagraph=true
skia_enable_skshaper=true
skia_enable_skunicode=true
'''.replace('SIM','true' if sim else 'false')
(out/'args.gn').write_text(args)
run('configure target-specific Skia',[skia/'bin/gn','gen',out],skia)
run('compile target-specific text and raster libraries',['ninja','-C',out,'-j','4','skia','skparagraph','skshaper','skunicode_icu','skunicode_core'],skia)
build=root/'build'
run('configure UIKit application host',['cmake','-S',source,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF',
 '-DCMAKE_SYSTEM_NAME=iOS','-DCMAKE_OSX_SYSROOT='+sysroot,'-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0',
 '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO','-DONEUI_BUILD_IOS_APPLICATION_HOST=ON','-DONEUI_BUILD_JSON=ON','-DONEUI_BUILD_VPN=ON','-DONEUI_VPN_JSON=ON','-DONEUI_ENABLE_YOGA=OFF',
 '-DONEUI_BUNDLED_SKIA_ROOT='+str(skia),'-DONEUI_BUNDLED_SKIA_OUT='+str(out)])
run('compile UIKit application host',['cmake','--build',build,'--parallel','4','--target','oneui_ios_application','oneui_json','oneui_vpn_json'])
sdk=root/'sdk';lib=sdk/'lib';lib.mkdir(parents=True,exist_ok=True)
archives=[build/('lib'+name+'.a') for name in ['oneui_ios_application','oneui_mobile_runtime','oneui_text','oneui_json','oneui_vpn','oneui_vpn_json']]
archives += [out/('lib'+name+'.a') for name in ['skia','skparagraph','skshaper','skunicode_icu','skunicode_core','harfbuzz','icu','expat','jpeg','jpeg12','jpeg16','png','webp','skcms','zlib','freetype2']]
for archive in archives:
 if not archive.is_file():raise RuntimeError('Missing SDK dependency: '+str(archive))
run('assemble static UIKit SDK',['xcrun','libtool','-static','-o',lib/'liboneui_ios_application.a',*archives])
run('strip distributable debug data',['xcrun','strip','-S',lib/'liboneui_ios_application.a'])
archs=subprocess.check_output(['xcrun','lipo','-archs',str(lib/'liboneui_ios_application.a')],text=True).split()
if archs!=['arm64']:
 thin=lib/'oneui-arm64-current.a'
 run('retain complete target slice',['xcrun','lipo',lib/'liboneui_ios_application.a','-thin','arm64','-output',thin])
 thin.replace(lib/'liboneui_ios_application.a')
shutil.copytree(source/'include',sdk/'include',dirs_exist_ok=True)
shutil.copytree(source/'platform/ios/vpn',sdk/'swift/vpn',dirs_exist_ok=True)
shutil.copy2(source/'bindings/go/oneuivpn/native/native_bridge.h',sdk/'include/oneui/vpn/native_bridge.h')
licenses=sdk/'licenses';licenses.mkdir(exist_ok=True)
notices={'oneui.txt':source/'LICENSE','yyjson.txt':source/'third_party/yyjson/LICENSE','skia.txt':skia/'LICENSE'}
for name,relative in {'harfbuzz':'harfbuzz/COPYING','icu':'icu/LICENSE','expat':'expat/expat/COPYING','freetype':'freetype/docs/FTL.TXT',
 'jpeg':'libjpeg-turbo/LICENSE.md','jpeg-ijg':'libjpeg-turbo/README.ijg','png':'libpng/LICENSE','webp':'libwebp/COPYING','zlib':'zlib/LICENSE'}.items():
 notices[name+'.txt']=skia/'third_party/externals'/relative
for name,path in notices.items():shutil.copy2(path,licenses/name)
package=lib/'cmake/OneUIiOS';package.mkdir(parents=True,exist_ok=True)
(package/'OneUIiOSConfig.cmake').write_text('''get_filename_component(_oneui_sdk "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_library(OneUI::ios_application STATIC IMPORTED)
set_target_properties(OneUI::ios_application PROPERTIES
 IMPORTED_LOCATION "${_oneui_sdk}/lib/liboneui_ios_application.a"
 INTERFACE_INCLUDE_DIRECTORIES "${_oneui_sdk}/include"
 INTERFACE_COMPILE_FEATURES cxx_std_17
 INTERFACE_COMPILE_DEFINITIONS "ONEUI_STATIC;ONEUI_MOBILE_STATIC"
 INTERFACE_LINK_LIBRARIES "-framework UIKit;-framework CoreGraphics;-framework CoreText;-framework CoreFoundation;-framework ImageIO;-framework QuartzCore")
''')
report={'slice':SLICE,'sdk':str(sdk),'checks':checks,'scope':'Compiled shared widgets, UIKit raster surface, lifecycle and native IME; runtime/device checks separate',
 'checked_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'oneui_mac_managed_bytes':audit()}
if source.resolve().parent==root.resolve() and (source/'.oneui-ios-fixture').is_file():shutil.rmtree(source)
report['source_fixture_exists']=source.exists()
(root/'verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--slice', choices=['simulator-arm64', 'device-arm64'], default='simulator-arm64')
    parser.add_argument('--ssh-host', default='oneui-mac')
    parser.add_argument('--local-managed-bytes', type=int, required=True)
    args = parser.parse_args()
    if args.local_managed_bytes < 1: raise RuntimeError('A current local artifact audit is required')
    root = Path(__file__).resolve().parents[1]
    files = [root / 'CMakeLists.txt']
    for directory in ['include', 'src', 'cmake', 'platform/ios/vpn', 'third_party/yyjson', 'third_party/patches']:
        files += [p for p in (root / directory).rglob('*') if p.is_file() and p.suffix in {'.h', '.hpp', '.inc', '.cpp', '.cc', '.c', '.m', '.mm', '.swift', '.cmake', '.in', '.patch'}]
    files += [root / 'scripts/apply-skia-patches.cmake', root / 'bindings/rust/oneui/src/ui/default.css']
    files += [root / 'LICENSE', root / 'third_party/yyjson/LICENSE', root / 'bindings/go/oneuivpn/native/native_bridge.h']
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode='w:gz') as archive:
        for path in sorted(set(files)): archive.add(path, arcname=path.relative_to(root).as_posix(), recursive=False)
    code = 'LOCAL_BYTES=' + str(args.local_managed_bytes) + ';SLICE=' + repr(args.slice) + '\n' + REMOTE
    remote = 'import base64;exec(base64.b64decode(' + repr(base64.b64encode(code.encode()).decode()) + '))'
    process = subprocess.run(['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes', args.ssh_host,
        '/usr/bin/python3 -c ' + shlex.quote(remote)], input=buffer.getvalue(), capture_output=True, timeout=1800)
    if process.returncode: raise RuntimeError(process.stderr.decode(errors='replace')[-8000:])
    report = json.loads(process.stdout)
    output = root / 'out/mobile-current/vpn-host/ios-application-verification.json'
    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: report[key] for key in ['slice', 'sdk', 'oneui_mac_managed_bytes', 'source_fixture_exists']}))


if __name__ == '__main__': main()
