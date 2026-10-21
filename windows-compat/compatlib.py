"""Compatibility contracts. Build identity is content-derived, never a label.

Standard library only. This code is a release/build tool, not a Win7 Python
runtime dependency. Product archives never contain this Python interpreter.
"""
import hashlib
import json
import re
from pathlib import Path, PurePosixPath


class ContractError(ValueError):
    pass


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def canonical(value):
    # RFC 8785 for our deliberately integer-only manifest schema. UTF-16 key
    # ordering matters for supplementary characters; Python sort_keys alone
    # is NOT interoperable with Go/Rust JCS. No float rounding is permitted.
    def encode(item):
        if item is None:
            return 'null'
        if isinstance(item, bool):
            return 'true' if item else 'false'
        if isinstance(item, int):
            if abs(item) > 9007199254740991:
                raise ContractError('manifest integer exceeds interoperable JSON range')
            return str(item)
        if isinstance(item, str):
            item.encode('utf-16-be')  # reject unpaired surrogates
            return json.dumps(item, ensure_ascii=False, separators=(',', ':'))
        if isinstance(item, list):
            return '[' + ','.join(encode(entry) for entry in item) + ']'
        if isinstance(item, dict) and all(isinstance(key, str) for key in item):
            keys = sorted(item, key=lambda key: key.encode('utf-16-be'))
            return '{' + ','.join(encode(key) + ':' + encode(item[key]) for key in keys) + '}'
        raise ContractError('manifest contains an unsupported JSON type')
    return encode(value).encode('utf-8')


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ContractError('duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8-sig'), object_pairs_hook=unique)


def safe_file(root, name):
    # Reject Windows ADS, device names, traversal, ambiguous separators/casing.
    if not isinstance(name, str) or '\\' in name or ':' in name:
        raise ContractError('invalid relative path')
    parts = name.split('/')
    if not name or any(part in ('', '.', '..') or part.endswith(('.', ' ')) for part in parts):
        raise ContractError('invalid relative path: ' + name)
    if PurePosixPath(name).is_absolute() or any(re.fullmatch(r'(con|prn|aux|nul|com[0-9]|lpt[0-9])(\..*)?', p, re.I) for p in parts):
        raise ContractError('reserved or absolute path: ' + name)
    root = Path(root).resolve()
    path = root.joinpath(*parts)
    for component in (path, *list(path.parents)[:len(parts)-1]):
        if component.is_symlink() or (hasattr(component, 'is_junction') and component.is_junction()):
            raise ContractError('linked path is not a locked input: ' + name)
    if not path.resolve().is_relative_to(root):
        raise ContractError('path escapes root: ' + name)
    return path


def inventory(root, exclude=()):
    root = Path(root).resolve()
    entries, names = [], set()
    for path in sorted(root.rglob('*')):
        name = path.relative_to(root).as_posix()
        safe_file(root, name)
        if not path.is_file() or name in exclude:
            continue
        if name.casefold() in names:
            raise ContractError('case-colliding file: ' + name)
        names.add(name.casefold())
        entries.append({'path': name, 'size': path.stat().st_size, 'sha256': sha256(path)})
    if not entries:
        raise ContractError('empty payload')
    return entries


def verify_files(root, entries, exclude=(), exact=True):
    if not isinstance(entries, list) or not entries:
        raise ContractError('missing locked files')
    names = set()
    for entry in entries:
        name = entry['path']
        path = safe_file(root, name)
        if name.casefold() in names:
            raise ContractError('duplicate file: ' + name)
        names.add(name.casefold())
        if not re.fullmatch('[0-9a-f]{64}', entry['sha256']):
            raise ContractError('invalid SHA256: ' + name)
        if not path.is_file() or sha256(path) != entry['sha256'] or path.stat().st_size != entry['size']:
            raise ContractError('locked input changed or missing: ' + name)
    if exact and {e['path'].casefold() for e in inventory(root, exclude)} != names:
        raise ContractError('unlisted payload files')


def version_tuple(value):
    if not isinstance(value, str) or not re.fullmatch(r'\d+\.\d+\.\d+', value):
        raise ContractError('OS version must be major.minor.build')
    return tuple(map(int, value.split('.')))


def choose_profile(os_version):
    version = version_tuple(os_version)
    if version[:2] == (10, 0) and version[2] >= 10240:
        return 'modern'
    if (version[:2] == (6, 1) and version[2] >= 7601) or (version[:2] == (6, 2) and version[2] >= 9200) or (version[:2] == (6, 3) and version[2] >= 9600):
        return 'legacy'
    raise ContractError('unsupported Windows version')


def identity(manifest):
    # Acceptance evidence is attached later; adding test results does not
    # change the binary identity. Everything that builds the payload does.
    data = {k: v for k, v in manifest.items() if k not in ('buildId', 'verification')}
    return hashlib.sha256(canonical(data)).hexdigest()


def validate_manifest(value, root=None, release=False, evidence_root=None, exceptions=None):
    required = {'schemaVersion', 'product', 'clientType', 'version', 'profile', 'architecture', 'minimumOS',
                'cpu', 'source', 'inputs', 'files', 'minimumUpdaterVersion', 'configVersion',
                'healthProtocolVersion', 'verification', 'buildId'}
    if not required <= value.keys() or value['schemaVersion'] != 1:
        raise ContractError('incomplete or unknown build manifest')
    for key in ('product', 'clientType', 'version'):
        if not isinstance(value[key], str) or not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9._-]*', value[key]):
            raise ContractError('invalid ' + key)
    if value['profile'] not in ('modern', 'legacy') or value['architecture'] not in ('x86', 'x86_64'):
        raise ContractError('unknown profile/architecture')
    minimum = '6.1.7601' if value['profile'] == 'legacy' else '10.0.0'
    if value['minimumOS'] != minimum or value['cpu'] != {'x86': 'sse2', 'x86_64': 'x86-64-v1'}[value['architecture']]:
        raise ContractError('baseline mismatch')
    if not re.fullmatch('[0-9a-f]{40}', value['source'].get('commit', '')) or not isinstance(value['source'].get('dirty'), bool):
        raise ContractError('missing source identity')
    if not value['inputs'] or any(not re.fullmatch('[0-9a-f]{64}', item.get('sha256', '')) for item in value['inputs']):
        raise ContractError('missing hashed build inputs')
    if not isinstance(value['files'], list) or not value['files']:
        raise ContractError('empty build manifest payload')
    names = set()
    for item in value['files']:
        safe_file('.', item.get('path', ''))
        if item['path'].casefold() in names or not re.fullmatch('[0-9a-f]{64}', item.get('sha256', '')) or not isinstance(item.get('size'), int) or item['size'] < 0:
            raise ContractError('invalid/duplicate payload entry')
        names.add(item['path'].casefold())
    if not isinstance(value['verification'], dict):
        raise ContractError('invalid verification records')
    if value['buildId'] != identity(value):
        raise ContractError('build identity mismatch')
    if root is not None:
        verify_files(root, value['files'], exclude=('build-manifest.json',))
    verification = value['verification']
    if release:
        if value['source']['dirty']:
            raise ContractError('uncommitted source cannot be a formal release')
        for check in ('build', 'imports', 'native', 'business', 'trust'):
            records = verification.get(check, [])
            if not records:
                raise ContractError('missing acceptance: ' + check)
            for record in records:
                deferred = record.get('status') == 'deferred'
                if record.get('buildId') != value['buildId']:
                    raise ContractError('unverified build: ' + check)
                if deferred:
                    validate_native_exception(value, check, record, exceptions)
                elif record.get('status') != 'passed':
                    raise ContractError('unverified build: ' + check)
                if not re.fullmatch('[0-9a-f]{64}', record.get('sha256', '')):
                    raise ContractError('unhashed evidence: ' + check)
                if evidence_root is None or sha256(safe_file(evidence_root, record['path'])) != record['sha256']:
                    raise ContractError('missing/changed acceptance evidence: ' + check)
                if check in ('native', 'business', 'trust'):
                    if not record.get('systemId') or record.get('processArchitecture') != value['architecture']:
                        raise ContractError('acceptance lacks exact OS/architecture: ' + check)
                    if not deferred:
                        if not record.get('osVersion'):
                            raise ContractError('acceptance lacks observed OS: ' + check)
                        version_tuple(record['osVersion'])
                    elif record.get('osVersion'):
                        raise ContractError('deferred acceptance cannot invent an observed OS')
    return value


def validate_native_exception(manifest, check, record, exceptions):
    # A native-run exception NEVER exempts compilation, imports, inventory or
    # cryptographic verification. Trust still needs a real verification record.
    if check not in ('native', 'business') or manifest['architecture'] != 'x86_64':
        raise ContractError('this release check cannot be deferred: ' + check)
    if not isinstance(exceptions, dict) or exceptions.get('schemaVersion') != 1:
        raise ContractError('missing separately approved release exceptions')
    system = record.get('systemId', '')
    if not system.endswith('-x86_64'):
        raise ContractError('native exception requires an exact 64-bit system')
    matches = [item for item in exceptions.get('exceptions', [])
               if item.get('id') == record.get('exceptionId') and
               item.get('product') == manifest['product'] and
               item.get('clientType') == manifest['clientType'] and
               item.get('version') == manifest['version'] and
               item.get('profile') == manifest['profile'] and
               item.get('architecture') == manifest['architecture'] and
               item.get('buildId') == manifest['buildId'] and
               system in item.get('systems', []) and check in item.get('checks', []) and
               item.get('approvedBy') == 'product-owner' and
               item.get('authorization') and item.get('reason')]
    if len(matches) != 1:
        raise ContractError('no unique owner approval for this exact build and check')


def verify_lock(lock, foundation, inputs=None):
    if lock.get('schemaVersion') != 1:
        raise ContractError('unknown lock version')
    verify_files(foundation, lock['foundation']['files'], exact=False)
    # A new Python module could shadow hashlib/json before our import checks.
    # Trust bootstraps check this set before invoking Python as well.
    locked_python = {e['path'].casefold() for e in lock['foundation']['files'] if e['path'].endswith('.py')}
    actual_python = {p.relative_to(foundation).as_posix().casefold() for p in Path(foundation).rglob('*.py')
                     if not any(part in ('.git', '__pycache__') for part in p.relative_to(foundation).parts)}
    if actual_python != locked_python:
        raise ContractError('unlisted or missing foundation Python module')
    for key, item in lock.get('inputs', {}).items():
        if inputs is None or key not in inputs:
            raise ContractError('missing explicit input root: ' + key)
        if item.get('status') != 'locked':
            raise ContractError('input not qualified/locked: ' + key)
        verify_files(inputs[key], item['files'], exact=False)
        if item.get('manifest'):
            metadata = load(safe_file(inputs[key], item['manifest']))
            verify_files(inputs[key], metadata['files'], exclude=(item['manifest'],))
        else:
            verify_files(inputs[key], item['files'])
