"""Build WYC Windows variants from explicit, locked inputs. Never publish."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compatlib import ContractError, identity, inventory, load, sha256, validate_manifest, verify_lock
from windows_pe import PE
from audit_windows import audit


def run(args, **kwargs):
    subprocess.run(list(map(str, args)), check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', required=True)
    parser.add_argument('--inputs', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--profile', choices=['modern', 'legacy'], required=True)
    parser.add_argument('--architecture', choices=['x86', 'x86_64'], required=True)
    parser.add_argument('--client', choices=['gui', 'service'], required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--reference')
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', args.version):
        raise ContractError('invalid product version')
    project = Path(args.project).resolve()
    foundation = Path(__file__).resolve().parents[1]
    roots = load(args.inputs)
    lock = load(project / 'compat.lock.json')
    keys = ['go-' + args.profile]
    if args.client == 'gui':
        keys += ['oneui-' + args.profile + '-' + args.architecture, 'mingw-' + args.architecture, 'oneupdate-' + args.architecture]
    if any(key not in lock['inputs'] for key in keys):
        raise ContractError('variant inputs are not yet locked: ' + ', '.join(key for key in keys if key not in lock['inputs']))
    lock['inputs'] = {key: lock['inputs'][key] for key in keys}
    if any(key not in roots or not Path(roots[key]).is_absolute() for key in keys):
        raise ContractError('every build input requires an explicit absolute root')
    verify_lock(lock, foundation, roots)
    output = Path(args.output).resolve()
    archive_path = Path(str(output) + '.zip')
    if output.exists() or archive_path.exists():
        raise ContractError('choose a new output; previous packages are never replaced')
    go_root = Path(roots[keys[0]])
    go_metadata = load(go_root / 'toolchain-manifest.json')
    if go_metadata['version'] != '1.25.14' or go_metadata['profile'] != args.profile:
        raise ContractError('wrong Go toolchain profile/version')
    environment = dict(os.environ)
    for name in ('GOFLAGS', 'CGO_CFLAGS', 'CGO_CPPFLAGS', 'CGO_CXXFLAGS', 'CGO_LDFLAGS', 'CC', 'CXX', 'GOARM'):
        environment.pop(name, None)
    environment.update(GOTOOLCHAIN='local', GOENV='off', GOWORK='off', GOROOT=str(go_root),
                       GOOS='windows', GOARCH={'x86': '386', 'x86_64': 'amd64'}[args.architecture],
                       GO386='sse2', GOAMD64='v1', CGO_ENABLED='0',
                       GOCACHE=str(output.parent / '.gocache'))
    backend = project / 'backend'
    output.mkdir(parents=True)
    executable = output / ('wyc-gui.exe' if args.client == 'gui' else 'wyc-client.exe')
    commit = subprocess.check_output(['git', '-C', str(project), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = bool(subprocess.check_output(['git', '-C', str(project), 'status', '--porcelain'], text=True))
    package = './cmd/wyc-client'
    flags = '-s -w'
    if args.client == 'service':
        flags += ' -X main.version=' + args.version + ' -X main.commit=' + commit
    else:
        code = (backend / 'internal/guiapp/client_identity.go').read_text(encoding='utf-8')
        match = re.search(r'const appClientVersion = "([^"]+)"', code)
        if not match or match[1] != args.version:
            raise ContractError('GUI version must match appClientVersion and Windows resources')
        flags += ' -H=windowsgui'
    temporary = None
    try:
        if args.client == 'gui':
            ui_root = Path(roots[keys[1]])
            ui = load(ui_root / 'sdk-manifest.json')
            if (ui['profile'], ui['architecture'], ui['configuration']) != (args.profile, args.architecture, 'Release'):
                raise ContractError('OneUI SDK profile/architecture mismatch')
            mingw = Path(roots[keys[2]])
            compiler = load(mingw / 'toolchain-manifest.json')
            if compiler['architecture'] != args.architecture or compiler['crt'] != 'msvcrt':
                raise ContractError('CGO requires the locked architecture-matched MSVCRT toolchain')
            toolbin = mingw / 'bin'
            environment.update(CGO_ENABLED='1', CC=str(toolbin/'gcc.exe'), CXX=str(toolbin/'g++.exe'),
                               PATH=str(toolbin)+os.pathsep+environment.get('PATH',''))
            bridge = output.parent / (output.name + '-link-inputs')
            if bridge.exists():
                raise ContractError('link input staging directory already exists')
            bridge.mkdir()
            shutil.copy2(ui_root/'bin/oneui.dll', bridge/'oneui.dll')
            exports = [name for name in PE(bridge/'oneui.dll').exports() if name.startswith('oneui_') and re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name)]
            if not exports:
                raise ContractError('OneUI C ABI exports are missing')
            (bridge/'oneui.def').write_text('LIBRARY oneui.dll\nEXPORTS\n'+'\n'.join(sorted(exports))+'\n',encoding='ascii')
            run([toolbin/'dlltool.exe', '-d', 'oneui.def', '-D', 'oneui.dll', '-l', 'liboneui.dll.a'], cwd=bridge, env=environment)
            cpu_flags = ' -march=pentium4 -msse2 -mno-avx' if args.architecture == 'x86' else ' -march=x86-64 -mno-avx'
            environment['CGO_CFLAGS'] = shlex.quote('-I' + str(ui_root/'include').replace('\\','/')) + cpu_flags
            environment['CGO_CXXFLAGS'] = environment['CGO_CFLAGS']
            environment['CGO_LDFLAGS'] = shlex.quote('-L' + str(bridge).replace('\\','/')) + ' -loneui -static-libgcc -static-libstdc++'
            # A fixed, exclusively-created build directory keeps the import
            # path reproducible and leaves the real app resources untouched.
            scratch = backend / 'cmd/.compat-gui-build'
            scratch.mkdir()  # fail if another build/user owns this name
            temporary = scratch  # ownership begins only AFTER mkdir succeeds
            for source in (backend/'cmd/wyc-gui').iterdir():
                if source.suffix in ('.go', '.rc', '.ico', '.manifest'):
                    shutil.copy2(source, temporary/source.name)
            target = 'pe-i386' if args.architecture == 'x86' else 'pe-x86-64'
            run([toolbin/'windres.exe','--input','app.rc','--output','zicon_windows_'+environment['GOARCH']+'.syso','--output-format','coff','--target',target],cwd=temporary,env=environment)
            package = './cmd/.compat-gui-build'
            shutil.copy2(ui_root/'bin/oneui.dll', output/'oneui.dll')
            updater_root = Path(roots[keys[3]])
            shutil.copy2(updater_root/'oneupdate_ffi.dll', output/'oneupdate_ffi.dll')
            (output/'runtime-files.txt').write_text('oneui.dll\noneupdate_ffi.dll\n',encoding='utf-8')
            if (backend/'assets/gui').is_dir():
                shutil.copytree(backend/'assets/gui',output/'assets/gui')
            shutil.copytree(ui_root/'licenses',output/'licenses/oneui')
        command = [go_root/'bin/go.exe','build','-mod=readonly','-trimpath','-ldflags',flags,'-o',executable]
        if args.client == 'gui':
            command += ['-tags','wycgui']
        run([*command, package], cwd=backend, env=environment)
    finally:
        # Only our exclusively-created, non-linked build scratch is removable.
        if temporary is not None and temporary.is_dir() and not temporary.is_symlink():
            if temporary.resolve() != backend.resolve()/'cmd/.compat-gui-build':
                raise ContractError('scratch path changed; refusing cleanup')
            shutil.rmtree(temporary)
    expected = 'x86' if args.architecture == 'x86' else 'x64'
    binaries = [p for p in output.rglob('*') if p.suffix.lower() in ('.exe','.dll')]
    if any(PE(path).arch != expected for path in binaries):
        raise ContractError('payload mixes architectures')
    if args.client == 'service':
        readme = (project/'docs/wyc-client-package-readme.md').read_text(encoding='utf-8').split('\n', 1)[1]
        (output/'README.md').write_text('# 网云穿无界面客户端 '+args.version+'（兼容工程候选）\n\n'
            +'构建配置：'+args.profile+'；程序架构：'+args.architecture+'。这不是正式上线包。完整服务安装、人工升级、回滚及业务验收尚未完成，请勿直接替换生产服务。\n'
            +readme,encoding='utf-8')
        shutil.copy2(project/'docs/wyc-client-config.example.json',output/'CONFIG.example.json')
    else:
        (output/'README.md').write_text('# 网云穿 GUI '+args.version+'（兼容工程候选）\n\n'
            +'构建配置：'+args.profile+'；程序架构：'+args.architecture+'。\n\n'
            +'解压整个目录后运行 wyc-gui.exe，不要单独复制 EXE。该普通便携候选不安装后台 Agent，也不替换现有客户端。\n\n'
            +'Win7 x64、Win8/8.1、完整业务与更新闭环仍待验收。本包不构成已支持声明，不得上传为正式更新。\n\n'
            +'本软件包含 FreeType 项目组件；相关版权和许可位于 licenses/oneui。完整制品清单及哈希见 build-manifest.json。\n',encoding='utf-8')
    # Preserve the precise build recipe as payload provenance, rather than
    # recording only the Go/SDK leaf hashes and losing the foundation version.
    shutil.copy2(project/'compat.lock.json',output/'build-input-lock.json')
    (output/'licenses').mkdir(exist_ok=True)
    shutil.copy2(go_root/'LICENSE',output/'licenses/go-BSD.txt')
    if args.profile == 'legacy':
        shutil.copy2(foundation/'patches/LICENSE-go-win7.txt',output/'licenses/go-compat-BSD.txt')
    evidence = output.parent / (output.name+'-evidence')
    evidence.mkdir()
    if args.reference:
        result = audit(binaries, load(args.reference))
        (evidence/'imports.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        if not result['passed']:
            raise ContractError('import audit failed; no archive produced: '+str(result['errors']))
    manifest = dict(schemaVersion=1,product='wyc',clientType=args.client,version=args.version,
                    profile=args.profile,architecture=args.architecture,
                    minimumOS='6.1.7601' if args.profile=='legacy' else '10.0.0',
                    cpu='sse2' if args.architecture=='x86' else 'x86-64-v1',
                    source={'commit':commit,'dirty':dirty},
                    inputs=[dict(name=key,sha256=lock['inputs'][key]['files'][0]['sha256']) for key in keys],
                    minimumUpdaterVersion='0.2.0',
                    configVersion=1,healthProtocolVersion=2,verification={},files=inventory(output))
    manifest['buildId']=identity(manifest)
    validate_manifest(manifest,output)
    (output/'build-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    with zipfile.ZipFile(archive_path,'x',zipfile.ZIP_DEFLATED) as archive:
        for path in output.rglob('*'):
            if path.is_file():
                archive.write(path,path.relative_to(output).as_posix())
    print('CANDIDATE '+str(archive_path))
    print('SHA256 '+sha256(archive_path))


if __name__ == '__main__':
    try:
        main()
    except (ContractError, OSError, subprocess.CalledProcessError) as error:
        print('BUILD BLOCKED: '+str(error),file=sys.stderr)
        sys.exit(1)
