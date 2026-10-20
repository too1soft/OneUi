"""Seal exact native compiler/updater inputs; acceptance remains independent."""
import argparse
import json
from pathlib import Path
import shutil
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from compatlib import inventory, ContractError

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',required=True)
p.add_argument('--output',required=True)
p.add_argument('--kind',choices=['mingw','oneupdate'],required=True)
p.add_argument('--architecture',choices=['x86','x86_64'],required=True)
p.add_argument('--packages',help='compiler package download directory with signed package files')
a=p.parse_args()
source,output=Path(a.source).resolve(),Path(a.output).resolve()
if output.exists():raise ContractError('output exists')
if a.kind=='mingw':
    for tool in ('gcc.exe','g++.exe','windres.exe','dlltool.exe'):
        if not (source/'bin'/tool).is_file():raise ContractError('compiler input missing: '+tool)
    shutil.copytree(source,output)
    metadata={'schemaVersion':1,'kind':'mingw','architecture':a.architecture,'crt':'msvcrt',
              'acceptance':'candidate-until-final-PE-and-native-tests','files':inventory(output)}
    if a.packages:metadata['sourcePackages']=inventory(a.packages)
else:
    output.mkdir(parents=True)
    for name in ('oneupdate-agent.exe','oneupdate-installer.exe','oneupdate_ffi.dll'):
        shutil.copy2(source/name,output/name)
    metadata={'schemaVersion':1,'kind':'oneupdate','architecture':a.architecture,'profile':'legacy',
              'version':'0.1.0','acceptance':'candidate-not-update-loop-certified','files':inventory(output)}
(output/'toolchain-manifest.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
print(output)
