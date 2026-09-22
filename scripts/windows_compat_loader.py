"""Load the pinned shared auditor without executing an unverified copy."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path


def load_shared(filename):
    project = Path(__file__).resolve().parents[1]
    root = Path(os.environ.get('WINDOWS_COMPAT_ROOT', str(project.parent/'windows-compat'))).resolve()
    lock = json.loads((project/'compat.lock.json').read_text(encoding='utf-8'))
    entries = lock['foundation']['files']
    if filename not in {item['path'] for item in entries}:
        raise RuntimeError('shared auditor is absent from compatibility lock')
    for item in entries:
        path = (root/item['path']).resolve()
        if not path.is_relative_to(root) or not path.is_file():
            raise RuntimeError('missing/escaping compatibility foundation input')
        if hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            raise RuntimeError('compatibility foundation changed: '+item['path'])
    spec = importlib.util.spec_from_file_location('_oneui_shared_'+filename.replace('.','_'), root/filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
