"""Package both OneUI ABIs from its generated link closure, never guessed libs."""
import argparse
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compatlib import ContractError, inventory, load, sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True)
    parser.add_argument('--build', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--licenses', required=True, help='reviewed complete OneUI license directory')
    args = parser.parse_args()
    source, build, output = map(lambda p: Path(p).resolve(), (args.source, args.build, args.output))
    if output.exists() or output.with_suffix('.zip').exists():
        raise ContractError('output exists; previous SDKs are never replaced')
    recipe = load(build / 'sdk-inputs-Release.json')
    if recipe.get('configuration') != 'Release' or recipe.get('schemaVersion') != 1:
        raise ContractError('only a known Release recipe can be packaged')
    cache = (build / 'CMakeCache.txt').read_text()
    if 'ONEUI_ENABLE_TEST_FRAME_CAPTURE:BOOL=OFF' not in cache:
        raise ContractError('test frame capture must be disabled')
    if recipe['profile'] == 'legacy' and '14.44.35207' not in recipe['compiler']:
        raise ContractError('legacy toolset mismatch')
    archives = list(dict.fromkeys(map(Path, recipe['archives'])))
    if len({p.name.lower() for p in archives}) != len(archives):
        raise ContractError('colliding static archive names')
    if recipe['profile'] == 'legacy' and 'freetype2.lib' not in {p.name for p in archives}:
        raise ContractError('missing legacy FreeType closure')
    licenses = Path(args.licenses)
    required = ['oneui-MIT.txt', 'skia-BSD.txt', 'icu.txt', 'harfbuzz.txt', 'freetype-FTL.txt',
                'freetype-license-selection.txt', 'freetype-bdf.txt', 'freetype-pcf.txt',
                'freetype-gzip-notice.txt', 'harfbuzz-ms-use.txt', 'libjpeg-turbo.md',
                'libjpeg-IJG.txt', 'libpng.txt', 'libwebp.txt', 'libwebp-patents.txt', 'zlib.txt', 'expat.txt']
    for path in [*archives, build / 'oneui.dll', build / 'oneui.lib', *[licenses / n for n in required]]:
        if not path.is_file():
            raise ContractError('SDK input missing: ' + str(path))
    for folder in ('bin', 'lib', 'include', 'licenses'):
        (output / folder).mkdir(parents=True)
    for path in archives:
        shutil.copy2(path, output / 'lib' / path.name)
    shutil.copy2(build / 'oneui.dll', output / 'bin')
    shutil.copy2(build / 'oneui.lib', output / 'lib')
    shutil.copytree(source / 'include', output / 'include', dirs_exist_ok=True)
    for name in required:
        shutil.copy2(licenses / name, output / 'licenses')
    # Machine-consumable, SDK-relative paths: no dependency on the build PC.
    static_names = [p.stem for p in archives]
    (output / 'static-libraries.txt').write_text('\n'.join(static_names) + '\n', encoding='utf-8')
    (output / 'system-libraries.txt').write_text('\n'.join(recipe['systemLibraries']) + '\n', encoding='utf-8')
    manifest = {k: recipe[k] for k in ('schemaVersion', 'profile', 'architecture', 'configuration')}
    manifest.update({'kind': 'oneui-sdk', 'crt': 'static', 'staticLibraries': static_names,
                     'systemLibraries': recipe['systemLibraries'],
                     'sourceCommit': subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip(),
                     'sourceDirty': bool(subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True)),
                     'sourceInputs': inventory(source / 'include'),
                     'compilerSha256': sha256(recipe['compiler']),
                     'recipeSha256': sha256(build / 'sdk-inputs-Release.json'),
                     'runtimeAcceptance': 'pending-product-integration',
                     'files': inventory(output)})
    (output / 'sdk-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    with zipfile.ZipFile(output.with_suffix('.zip'), 'x', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output.rglob('*')):
            if path.is_file():
                archive.write(path, path.relative_to(output).as_posix())
    print(output)
    print('SHA256 ' + sha256(output.with_suffix('.zip')))


if __name__ == '__main__':
    main()
