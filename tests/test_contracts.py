import copy
import tempfile
import unittest
from pathlib import Path
from compatlib import *


class Contracts(unittest.TestCase):
    def test_profiles(self):
        for version in ('6.1.7601', '6.2.9200', '6.3.9600'):
            self.assertEqual(choose_profile(version), 'legacy')
        for version in ('10.0.10240', '10.0.22631'):
            self.assertEqual(choose_profile(version), 'modern')
        for version in ('6.1.7600', '6.0.6002', '5.1.2600', 'unknown', '6.4.0', '6.2.0', '6.3.1', '10.0.0', '11.0.12345'):
            with self.assertRaises(ContractError):
                choose_profile(version)

    def test_paths(self):
        for name in ('../x', '/x', 'a/../x', 'C:/x', 'x:stream', 'a\\b', 'NUL.txt', 'a/COM1', 'x.', 'x ', 'a//b'):
            with self.assertRaises(ContractError, msg=name):
                safe_file('.', name)

    def test_hashes_and_extra_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'app.exe').write_bytes(b'payload')
            files = inventory(root)
            verify_files(root, files)
            (root / 'unexpected.dll').write_bytes(b'extra')
            with self.assertRaises(ContractError):
                verify_files(root, files)
            (root / 'app.exe').write_bytes(b'tampered')
            with self.assertRaises(ContractError):
                verify_files(root, files, exact=False)

    def test_manifest_is_not_runtime_evidence(self):
        value = dict(schemaVersion=1, product='wyc', clientType='service', version='2.4.3',
                     profile='legacy', architecture='x86', minimumOS='6.1.7601', cpu='sse2',
                     source={'commit': 'a'*40, 'dirty': False}, inputs=[{'sha256': 'b'*64}],
                     files=[{'path':'app.exe','size':1,'sha256':'c'*64}], minimumUpdaterVersion='0.1.0', configVersion=1,
                     healthProtocolVersion=2, verification={})
        value['buildId'] = identity(value)
        validate_manifest(value)
        with self.assertRaises(ContractError):
            validate_manifest(value, release=True)
        for key, new in (('profile', 'modern'), ('architecture', 'x86_64'), ('version', '2.4.4')):
            changed = copy.deepcopy(value)
            changed[key] = new
            with self.assertRaises(ContractError):
                validate_manifest(changed)

    def test_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bad.json'
            path.write_text('{"profile":"legacy","profile":"modern"}')
            with self.assertRaises(ContractError):
                load(path)

    def test_unqualified_input_blocks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'policy').write_bytes(b'policy')
            lock = {'schemaVersion': 1, 'foundation': {'files': inventory(root)},
                    'inputs': {'go': {'status': 'pending'}}}
            with self.assertRaisesRegex(ContractError, 'missing explicit'):
                verify_lock(lock, root)
            with self.assertRaisesRegex(ContractError, 'not qualified'):
                verify_lock(lock, root, {'go': root})


if __name__ == '__main__':
    unittest.main()
