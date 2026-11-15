"""Build/run the optional Android document source contracts in a separately owned package."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ['LOCALAPPDATA'])/'Android/sdk'
TOOLS = SDK/'build-tools/36.1.0'
JDK = Path('C:/Program Files/Eclipse Adoptium/jdk-17.0.18.8-hotspot/bin')
PACKAGE = 'net.too1.oneui.documents.contracts'


def run(args, timeout=45):
    return subprocess.run(list(map(str,args)), check=True, capture_output=True,
                          encoding='utf-8', errors='replace', timeout=timeout)


def build(output):
    classes, dex = output/'classes', output/'dex'
    dex.mkdir(parents=True,exist_ok=False)
    sources = sorted((ROOT/'platform/android/documents/java').rglob('*.java'))
    sources += sorted((ROOT/'tests/mobile/android-documents').glob('*.java'))
    android = SDK/'platforms/android-36/android.jar'
    run([JDK/'javac.exe','-encoding','UTF-8','-source','17','-target','17','-classpath',android,'-d',classes,*sources])
    run([JDK/'java.exe','-cp',TOOLS/'lib/d8.jar','com.android.tools.r8.D8','--min-api','28',
         '--lib',android,'--output',dex,*classes.rglob('*.class')])
    manifest = output/'AndroidManifest.xml'
    manifest.write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="net.too1.oneui.documents.contracts">
<uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
<application android:label="Owned document contracts" android:allowBackup="false">
<provider android:name="net.too1.oneui.documents.OwnedDocumentProvider" android:authorities="net.too1.oneui.documents.fixture" android:exported="false"/>
</application>
<instrumentation android:name="net.too1.oneui.documents.DocumentSourceContractRunner" android:targetPackage="net.too1.oneui.documents.contracts"/>
</manifest>''',encoding='utf-8')
    unsigned,aligned,signed=[output/name for name in ('unsigned.apk','aligned.apk','document-contracts.apk')]
    run([TOOLS/'aapt2.exe','link','-o',unsigned,'--manifest',manifest,'-I',android])
    with zipfile.ZipFile(unsigned,'a') as archive:archive.write(dex/'classes.dex','classes.dex')
    run([TOOLS/'zipalign.exe','-P','16','-f','4',unsigned,aligned])
    run([JDK/'java.exe','-jar',TOOLS/'lib/apksigner.jar','sign','--ks',Path.home()/'.android/debug.keystore',
         '--ks-pass','pass:android','--key-pass','pass:android','--out',signed,aligned])
    run([JDK/'java.exe','-jar',TOOLS/'lib/apksigner.jar','verify',signed])
    proof={'sources':[{'path':p.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sources],
           'apk_sha256':hashlib.sha256(signed.read_bytes()).hexdigest(),'fixture_provider':'same UID, no system SAF picker',
           'product_package_changed':False}
    (output/'build-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'sources':len(sources),'apk_sha256':proof['apk_sha256']}))


def execute(output, serial):
    def adb(*args):return run([SDK/'platform-tools/adb.exe','-s',serial,*args])
    assert adb('shell','id','-u').stdout.strip()=='2000','ordinary ADB UID required'
    apk=output/'document-contracts.apk'
    assert 'Success' in adb('install','-r',apk).stdout
    installed=adb('shell','pm','path',PACKAGE).stdout.strip().removeprefix('package:')
    assert adb('shell','sha256sum',installed).stdout.split()[0]==hashlib.sha256(apk.read_bytes()).hexdigest()
    assert 'Success' in adb('shell','pm','clear',PACKAGE).stdout
    result=adb('shell','am','instrument','-w','-r',PACKAGE+'/net.too1.oneui.documents.DocumentSourceContractRunner')
    (output/'instrumentation.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    fields=dict(re.findall(r'^INSTRUMENTATION_RESULT: ([^=]+)=(.*)$',result.stdout,re.MULTILINE))
    proof={'fields':fields,'instrumentation_ok':'INSTRUMENTATION_CODE: -1' in result.stdout,'ordinary_uid':2000,
           'fixture_provider':'same-UID ContentResolver; no system SAF grant or product UI'}
    (output/'result.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps(proof))
    assert proof['instrumentation_ok'] and fields.get('passed')=='true','Android document contracts failed'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['build','run'])
    parser.add_argument('--stage',type=Path,required=True)
    parser.add_argument('--attempt',type=int,required=True)
    parser.add_argument('--serial')
    args=parser.parse_args()
    stage=args.stage.resolve(strict=True)
    assert (stage/'.owner').read_text().strip()=='oneui-android-document-test'
    assert args.attempt>0
    output=stage/str(args.attempt)
    if args.mode=='build':output.mkdir(exist_ok=False);build(output)
    else:
        assert args.serial and re.fullmatch(r'emulator-[0-9]+',args.serial)
        execute(output,args.serial)


if __name__=='__main__':main()
