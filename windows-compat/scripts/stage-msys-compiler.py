"""Stage only the installed compiler package closure, not a whole MSYS tree."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from compatlib import ContractError, inventory

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--msys-root',required=True)
p.add_argument('--output',required=True)
a=p.parse_args()
msys,output=Path(a.msys_root).resolve(),Path(a.output).resolve()
if output.exists():raise ContractError('output exists')
pacman=msys/'usr/bin/pacman.exe'
prefix='mingw-w64-x86_64-'
packages=[prefix+s for s in ('gcc','binutils','crt','headers','gcc-libs','isl','gmp','mpfr','mpc','windows-default-manifest','winpthreads','libwinpthread','zlib','zstd','gettext-runtime','libiconv','tzdata')]
versions=subprocess.check_output([str(pacman),'-Q',*packages],text=True)
files=subprocess.check_output([str(pacman),'-Qql',*packages],text=True).splitlines()
output.mkdir(parents=True)
for raw in sorted(set(files)):
    if not raw.startswith('/mingw64/') or raw.endswith('/'):
        continue
    source=msys/raw.lstrip('/')
    if not source.is_file() or not source.resolve().is_relative_to(msys/'mingw64'):
        raise ContractError('missing/linked compiler package file: '+raw)
    destination=output/raw.removeprefix('/mingw64/')
    destination.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,destination)
metadata={'schemaVersion':1,'kind':'mingw','architecture':'x86_64','crt':'msvcrt',
          'sourcePackages':versions.splitlines(),'acceptance':'candidate-final-PE-audit-required',
          'files':inventory(output)}
(output/'toolchain-manifest.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
print(output)
