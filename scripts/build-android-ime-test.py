"""Build a small, separate Android instrumentation fixture using the existing lab classes."""
import argparse
from pathlib import Path
import subprocess
import zipfile

def run(*args): subprocess.run(list(map(str,args)),check=True)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk',type=Path,required=True);p.add_argument('--java-home',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1]
    source=root/'tests/mobile/android-ime';out=root/'out/mobile-current/ime-test';out.mkdir(exist_ok=True)
    tools=a.sdk/'build-tools/36.1.0';android=a.sdk/'platforms/android-36/android.jar'
    java=a.java_home/'bin/java.exe';classes=out/'classes';classes.mkdir(exist_ok=True)
    lab=root/'out/mobile-current/android-x86_64/apk/classes.jar'
    run(a.java_home/'bin/javac.exe','-encoding','UTF-8','-source','8','-target','8',
        '-bootclasspath',str(android)+';'+str(tools/'core-lambda-stubs.jar'),'-classpath',lab,
        '-d',classes,*sorted(source.glob('*.java')))
    jar=out/'classes.jar'
    with zipfile.ZipFile(jar,'w',zipfile.ZIP_DEFLATED) as z:
        for item in classes.rglob('*.class'):z.write(item,item.relative_to(classes).as_posix())
    run(java,'-Xmx128m','-Djava.io.tmpdir='+str(out),'-cp',tools/'lib/d8.jar','com.android.tools.r8.D8',
        '--lib',android,'--classpath',lab,'--min-api','28','--output',out,jar)
    unsigned=out/'unsigned.apk'
    run(tools/'aapt2.exe','compile','--dir',source/'res','-o',out/'resources.zip')
    run(tools/'aapt2.exe','link','-I',android,'--manifest',source/'AndroidManifest.xml',
        '--min-sdk-version','28','--target-sdk-version','36','-o',unsigned,out/'resources.zip')
    with zipfile.ZipFile(unsigned,'a',zipfile.ZIP_DEFLATED) as z:z.write(out/'classes.dex','classes.dex')
    run(java,'-Xmx128m','-jar',tools/'lib/apksigner.jar','sign','--ks',Path.home()/'.android/debug.keystore',
        '--ks-key-alias','androiddebugkey','--ks-pass','pass:android','--key-pass','pass:android',
        '--out',out/'input-tests.apk',unsigned)
    print(out/'input-tests.apk')
if __name__=='__main__':main()
