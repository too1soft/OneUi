"""Release transaction regressions; no SSH, builds, containers or real downloads."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile

from release_payload import module, validate_archive


def load_remote():
    spec = importlib.util.spec_from_file_location('release_remote', Path(__file__).with_name('release_remote.py'))
    result = importlib.util.module_from_spec(spec)
    if sys.platform == 'win32':
        with patch.dict(sys.modules, fcntl=types.SimpleNamespace()):
            spec.loader.exec_module(result)
    else:
        spec.loader.exec_module(result)
    return result


remote = load_remote()


def artifact(name, content):
    return dict(file=name, size=len(content), sha256=hashlib.sha256(content).hexdigest())


class Transactions(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='oneui-publish-test-')
        self.root = Path(self.temp.name)
        self.state = self.root / '.oneui-publish'
        self.stage = self.state / 'stage'
        self.stage.mkdir(parents=True)
        old_item = artifact('oneui-old.zip', b'old')
        new_item = artifact('oneui-new.zip', b'new')
        self.old = dict(schemaVersion=1, releases=[dict(version='old', artifacts=[old_item])])
        self.new = dict(schemaVersion=1, releases=[dict(version='new', artifacts=[new_item]), self.old['releases'][0]])
        self.old_bytes = json.dumps(self.old).encode()
        self.new_bytes = json.dumps(self.new).encode()
        (self.root / 'releases.json').write_bytes(self.old_bytes)
        (self.root / 'oneui-old.zip').write_bytes(b'old')
        (self.root / 'SHA256SUMS').write_bytes(b'old checksums')
        (self.stage / 'oneui-new.zip').write_bytes(b'new')
        (self.stage / 'releases.json').write_bytes(self.new_bytes)
        self.transaction = dict(token='test', baseHash=hashlib.sha256(self.old_bytes).hexdigest(),
                                newHash=hashlib.sha256(self.new_bytes).hexdigest())
        (self.stage / 'transaction.json').write_text(json.dumps(self.transaction))
        self.request = dict(token='test', version='new')

    def tearDown(self):
        self.temp.cleanup()

    def commit(self):
        return remote.commit(dict(container='oneui'), self.root, self.state, self.request)

    def test_success_preserves_historical_bytes_and_cleans_stage(self):
        with patch.object(remote, 'command', return_value='OK'), patch.object(remote, 'restart', return_value={'status': 'ok'}):
            self.assertEqual(self.commit()['status'], 'published')
        self.assertEqual((self.root / 'oneui-old.zip').read_bytes(), b'old')
        self.assertEqual((self.root / 'releases.json').read_bytes(), self.new_bytes)
        self.assertEqual((self.state / 'previous-releases.json').read_bytes(), self.old_bytes)
        self.assertFalse(self.stage.exists())

    def test_failed_restart_restores_catalog_and_checksums(self):
        with patch.object(remote, 'command', return_value='OK'), patch.object(remote, 'restart', side_effect=[RuntimeError('unhealthy'), {'status': 'ok'}]) as restart:
            with self.assertRaises(RuntimeError): self.commit()
            self.assertEqual(restart.call_count, 2)
        self.assertEqual((self.root / 'releases.json').read_bytes(), self.old_bytes)
        self.assertEqual((self.root / 'SHA256SUMS').read_bytes(), b'old checksums')
        self.assertFalse(self.stage.exists())

    def test_corrupt_upload_does_not_restart_or_change_catalog(self):
        (self.stage / 'oneui-new.zip').write_bytes(b'bad')
        with patch.object(remote, 'command') as command:
            with self.assertRaises(ValueError): self.commit()
            command.assert_not_called()
        self.assertEqual((self.root / 'releases.json').read_bytes(), self.old_bytes)

    def test_concurrent_catalog_change_is_not_overwritten(self):
        (self.root / 'releases.json').write_bytes(b'newer concurrent catalog')
        with self.assertRaises(ValueError): self.commit()
        self.assertEqual((self.root / 'releases.json').read_bytes(), b'newer concurrent catalog')

    def test_changed_history_is_rejected(self):
        self.new['releases'][1]['extra'] = 'changed'
        data = json.dumps(self.new).encode()
        (self.stage / 'releases.json').write_bytes(data)
        self.transaction['newHash'] = hashlib.sha256(data).hexdigest()
        (self.stage / 'transaction.json').write_text(json.dumps(self.transaction))
        with self.assertRaises(ValueError): self.commit()

    def test_path_escape_is_rejected(self):
        self.new['releases'][0]['artifacts'][0]['file'] = '../other-service.zip'
        with self.assertRaises(ValueError): remote.downloads(self.new)

    def test_rollback_cannot_overwrite_other_version_file(self):
        (self.root / 'oneui-new.zip').write_bytes(b'previously published different data')
        with self.assertRaises(ValueError): self.commit()

    def test_auto_rollback_cannot_undo_another_publishers_release(self):
        with patch.object(remote, 'command', return_value='OK'), patch.object(remote, 'restart', return_value={'status': 'ok'}):
            self.commit()
        with self.assertRaisesRegex(ValueError, 'Another deployment'):
            remote.rollback({}, self.root, self.state, 'a-different-transaction-hash')
        self.assertEqual((self.root / 'releases.json').read_bytes(), self.new_bytes)


class Payload(unittest.TestCase):
    def test_hidden_archive_member_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'x.zip'
            raw = json.dumps(dict(files=[dict(path='good', size=2, sha256=hashlib.sha256(b'ok').hexdigest())])).encode()
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('sdk-manifest.json', raw)
                archive.writestr('good', b'ok')
                archive.writestr('../bad', b'bad')
            with self.assertRaises(ValueError):
                validate_archive(path, dict(manifestSha256=hashlib.sha256(raw).hexdigest()))

    def test_partial_target_requires_both_linkages(self):
        catalog = module('make-release-catalog')
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'Incomplete'):
                catalog.build_release(Path(directory), Path(directory), '0.1.0-preview.2', 'preview',
                                      '2026-10-13', ['windows-modern-x86_64'])
            with self.assertRaisesRegex(ValueError, 'Select explicit'):
                catalog.build_release(Path(directory), Path(directory), '0.1.0-preview.2', 'preview', '2026-10-13', [])


if __name__ == '__main__':
    unittest.main()
