"""Seal explicit updater build outputs as candidate inputs; never publish."""
import argparse
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from compatlib import ContractError,inventory,load,sha256
from windows_pe import PE

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--built',required=True);p.add_argument('--output',required=True)
    p.add_argument('--product-lock',required=True);p.add_argument('--previous-lock-sha256',required=True)
    p.add_argument('--inputs',required=True);p.add_argument('--output-inputs',required=True)
    args=p.parse_args()
    built=Path(args.built).resolve();output=Path(args.output).resolve();lock=Path(args.product_lock).resolve()
    if sha256(lock)!=args.previous_lock_sha256.lower():raise ContractError('lock changed since review')
    if output.exists() or Path(args.output_inputs).exists():raise ContractError('candidate staging output already exists')
    value=load(lock);roots=load(args.inputs)
    for arch,target,pe_arch in [('x86','i686-win7-windows-msvc','x86'),('x86_64','x86_64-win7-windows-msvc','x64')]:
        root=output/('oneupdate-'+arch);root.mkdir(parents=True)
        for name in ['oneupdate-agent.exe','oneupdate-installer.exe','oneupdate_ffi.dll']:
            source=built/target/'release'/name
            if PE(source).arch!=pe_arch:raise ContractError('wrong updater architecture')
            shutil.copy2(source,root/name)
        metadata=dict(schemaVersion=1,kind='oneupdate',architecture=arch,profile='legacy',version='0.2.0',acceptance='candidate-not-update-loop-certified',files=inventory(root))
        (root/'toolchain-manifest.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf8')
        key='oneupdate-'+arch
        value['inputs'][key]=dict(status='locked',manifest='toolchain-manifest.json',files=inventory(root))
        roots[key]=str(root)
    lock.write_text(json.dumps(value,indent=2)+'\n',encoding='utf8')
    Path(args.output_inputs).write_text(json.dumps(roots,indent=2)+'\n',encoding='utf8')
    print('Candidate inputs sealed; native install/update acceptance is still required.')

if __name__=='__main__':
    try:main()
    except (OSError,ValueError,ContractError) as error:raise SystemExit(str(error))
