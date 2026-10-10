"""Disposable test directories inheriting the caller's writable-root ACL.

Python 3.14 uses a creator-only Windows ACL for mkdtemp(0700), which excludes
restricted build tokens. These fixtures contain no secrets; inherited workspace
permissions are intentional. Never change existing directory permissions.
"""
from contextlib import contextmanager
from pathlib import Path
import os
import shutil
import tempfile
import uuid


@contextmanager
def temporary_workspace(root=None):
    root = Path(root or os.environ.get('ONEUI_VIEWC_TEST_ROOT') or
                (Path(tempfile.gettempdir()) / 'oneui-viewc-tests')).resolve()
    root.mkdir(parents=True, exist_ok=True)
    directory = root / ('viewc-' + uuid.uuid4().hex)
    directory.mkdir()  # Inherit the ACL; no chmod or permission escalation.
    try:
        yield str(directory)
    finally:
        resolved = directory.resolve()
        if resolved.parent != root or directory.is_symlink():
            raise RuntimeError('Unexpected fixture cleanup path')
        shutil.rmtree(directory)
