"""Build the complete Java adapter from source, with a deterministic JAR."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def build_java_sdk(root, android_sdk, classes, jar):
    classes.mkdir(parents=True, exist_ok=True)
    marker = classes / 'oneui-java-sdk.json'
    if marker.is_file():
        previous = json.loads(marker.read_text(encoding='utf-8'))
        if previous.get('owner') != 'oneui-java-sdk':
            raise RuntimeError('Java output directory is not owned by this builder')
        for path in classes.rglob('*.class'):
            if not path.resolve().is_relative_to(classes.resolve()) or path.suffix != '.class':
                raise RuntimeError('Invalid previous Java output path')
            path.unlink()
    elif any(classes.rglob('*.class')):
        raise RuntimeError('Refusing to mix untracked class files into the SDK')
    marker.write_text(json.dumps({'owner': 'oneui-java-sdk', 'complete': False}) + '\n', encoding='utf-8')
    sources = sorted((root / 'platform/android/java').rglob('*.java'))
    if not sources:
        raise RuntimeError('No Java SDK sources found')
    platform = android_sdk / 'platforms/android-36/android.jar'
    lambdas = android_sdk / 'build-tools/36.1.0/core-lambda-stubs.jar'
    javac = Path(os.environ['JAVA_HOME']) / 'bin/javac.exe'
    subprocess.run([str(javac), '-encoding', 'UTF-8', '-source', '8', '-target', '8',
        '-bootclasspath', str(platform) + ';' + str(lambdas), '-d', str(classes),
        *map(str, sources)], check=True)
    outputs = sorted(classes.rglob('*.class'))
    jar.parent.mkdir(parents=True, exist_ok=True)
    temporary = jar.with_suffix('.jar.tmp')
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in outputs:
            info = zipfile.ZipInfo(path.relative_to(classes).as_posix(), (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())
    temporary.replace(jar)
    proof = {'owner': 'oneui-java-sdk', 'complete': True, 'sources': {
        path.relative_to(root).as_posix(): digest(path) for path in sources},
        'classes': {path.relative_to(classes).as_posix(): digest(path) for path in outputs},
        'platformSHA256': digest(platform), 'lambdaStubsSHA256': digest(lambdas),
        'compiler': subprocess.check_output([str(javac), '-version'], text=True).strip(),
        'jarSHA256': digest(jar)}
    marker.write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8')
    return proof
