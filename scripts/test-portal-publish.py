"""Exercise production transaction success, rollback and concurrency on fixtures."""
import importlib.util
import json
from pathlib import Path
import tempfile
import os
import sys
import types
import unittest
from unittest.mock import patch
import uuid
import io

if os.name == 'nt':
    sys.modules['fcntl'] = types.SimpleNamespace(LOCK_EX=1, LOCK_NB=2, flock=lambda *_: None)

spec = importlib.util.spec_from_file_location('portal_remote', Path(__file__).with_name('portal_remote.py'))
remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(remote)
restart_real=remote.restart


class Transactions(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config = dict(directory='/apps/oneui', container='oneui')
        self.old = dict(schemaVersion=1, releases=[dict(version='old', artifacts=[])])
        (self.root/'releases.json').write_text(json.dumps(self.old))
        (self.root/'oneui').write_bytes(b'old program')
        self.token = str(uuid.uuid4())
        self.addCleanup(patch.stopall)
        patch.object(remote, 'Path', side_effect=lambda p: self.root if p == '/apps/oneui' else Path(p)).start()
        def command(args):
            if args[:2] == ['docker','inspect']:
                return json.dumps([dict(Type='bind', Source=str(self.root), Destination='/app')])
            if args[-1]=='--check':
                return 'OK: 1 releases, 1 verified downloads'
            return ''
        patch.object(remote, 'command', side_effect=command).start()
        self.restart = patch.object(remote, 'restart', return_value={'status':'ok'}).start()

    def call(self, action, **values):
        return remote.main(dict(config=self.config, action=action, **values))

    def stage(self):
        binary='oneui-site-0.1.0-preview.4-linux-amd64'
        path=Path(self.call('begin',token=self.token,base=self.call('inspect')['stamp'],binary=binary,bytes=100)['stage'])
        (path/binary).write_bytes(b'new program')
        (path/'releases.json').write_text(json.dumps(self.old))
        (path/'plugins.json').write_text('{"schemaVersion":1,"packages":[]}')
        tools=dict(schemaVersion=1,tools=[dict(file=binary,size=11,sha256=remote.digest(path/binary))])
        (path/'tools.json').write_text(json.dumps(tools))
        return path

    def test_success_keeps_backup_and_cleans_stage(self):
        stage=self.stage()
        self.assertEqual(self.call('commit',token=self.token)['status'],'published')
        self.assertEqual((self.root/'oneui').read_bytes(),b'new program')
        self.assertEqual((self.root/'.oneui-portal/previous-oneui').read_bytes(),b'old program')
        self.assertFalse(stage.exists())
        self.call('rollback')
        self.assertEqual((self.root/'oneui').read_bytes(),b'old program')

    def test_failed_restart_restores_binary_and_all_catalogs(self):
        stage=self.stage()
        self.restart.side_effect=[RuntimeError('failed candidate'),{'status':'ok'}]
        with self.assertRaises(RuntimeError):self.call('commit',token=self.token)
        self.assertEqual((self.root/'oneui').read_bytes(),b'old program')
        self.assertEqual(json.loads((self.root/'releases.json').read_text()),self.old)
        self.assertFalse((self.root/'plugins.json').exists())
        self.assertFalse((self.root/'tools.json').exists())
        self.assertFalse(stage.exists())

    def test_changed_history_and_concurrent_publisher_are_rejected(self):
        stage=self.stage()
        (stage/'releases.json').write_text('{"schemaVersion":1,"releases":[]}')
        with self.assertRaisesRegex(ValueError,'history'):self.call('commit',token=self.token)
        self.assertEqual((self.root/'oneui').read_bytes(),b'old program')
        (self.root/'releases.json').write_text('{"schemaVersion":1,"releases":[]}')
        with self.assertRaisesRegex(ValueError,'concurrent'):self.call('commit',token=self.token)

    def test_tampered_cli_is_rejected_before_restart(self):
        stage=self.stage();(stage/'oneui-site-0.1.0-preview.4-linux-amd64').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError,'integrity'):self.call('commit',token=self.token)
        self.restart.assert_not_called()

    def test_optional_null_is_equivalent_but_real_changes_are_not(self):
        self.assertEqual(remote.canonical({'manifest':{}}),remote.canonical({'manifest':{'services':None}}))
        self.assertNotEqual(remote.canonical({'version':1}),remote.canonical({'version':2}))

    def test_retry_reuses_identical_immutable_files(self):
        name='oneui-site-0.1.0-preview.4-linux-amd64'
        file=self.root/name;file.write_bytes(b'new program')
        result=self.call('begin',token=self.token,base=self.call('inspect')['stamp'],binary=name,bytes=100,
            files={name:dict(size=11,sha256=remote.digest(file))})
        self.assertEqual(result['present'],[name])
        self.call('abort',token=self.token)
        self.assertEqual(self.call('abort',token=self.token)['status'],'already-clean')

    def test_full_integrity_scan_may_take_longer_than_ten_seconds(self):
        elapsed=[0.0]
        def request(url,timeout):
            if elapsed[0]<11:
                raise ConnectionResetError('Download integrity scan still running')
            value={'status':'ok'} if url.endswith('/healthz') else self.old
            return io.BytesIO(json.dumps(value).encode())
        with patch.object(remote.time,'monotonic',side_effect=lambda:elapsed[0]), \
             patch.object(remote.time,'sleep',side_effect=lambda seconds:elapsed.__setitem__(0,elapsed[0]+seconds)), \
             patch.object(remote.urllib.request,'urlopen',side_effect=request):
            self.assertEqual(restart_real(self.config,{'releases.json':self.old})['status'],'ok')
        self.assertEqual(elapsed[0],11)


if __name__ == '__main__':
    unittest.main()
