"""Executable routing specification. Not a replacement for signed server/Agent checks.

Inputs must come from a verified release inventory. Legacy rows without that
inventory are deliberately not inferred from their old min_os_version value.
"""
import re
from compatlib import ContractError, choose_profile, version_tuple

CAPABILITY = 'windows_compat_v2'

def product_version(value):
    match = re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?', value)
    if not match:
        raise ContractError('invalid product version')
    pre = match[4]
    identifiers = []
    if pre:
        for part in pre.split('.'):
            if not part or (part.isdigit() and len(part) > 1 and part[0] == '0'):
                raise ContractError('invalid prerelease identifier')
            identifiers.append((0, int(part)) if part.isdigit() else (1, part))
    return (*map(int, match.group(1, 2, 3)), 1 if pre is None else 0, tuple(identifiers))

def system_id(os_version, native_arch):
    version = version_tuple(os_version)
    choose_profile(os_version)
    if native_arch not in ('x86', 'x86_64'):
        raise ContractError('unsupported native architecture')
    family = {(6, 1): 'win7-sp1-original', (6, 2): 'win8', (6, 3): 'win81'}.get(version[:2])
    if family is None:
        family = 'win11' if version >= (10, 0, 22000) else 'win10'
    return family + '-' + native_arch

def select_release(releases, request):
    """Filter all compatible candidates FIRST, then order and apply rollout.

    No automatic installed-profile/architecture migration. New installations
    need an explicit product architecture from the matching installer.
    Returns None if no offer; malformed identity raises, never guesses.
    """
    if CAPABILITY not in request.get('capabilities', []):
        raise ContractError('verified bridge/manual repair installer required')
    required = ('product', 'clientType', 'channel', 'profile', 'architecture', 'nativeArchitecture', 'osVersion')
    if any(not request.get(key) for key in required):
        raise ContractError('incomplete compatibility identity')
    system = system_id(request['osVersion'], request['nativeArchitecture'])
    arch = request['architecture']
    if arch not in ('x86', 'x86_64') or (arch == 'x86_64' and request['nativeArchitecture'] != 'x86_64'):
        raise ContractError('wrong program architecture for OS')
    profile = request['profile']
    if profile not in ('legacy', 'modern') or (profile == 'modern' and choose_profile(request['osVersion']) != 'modern'):
        raise ContractError('wrong profile for OS')
    if request.get('installedVersion') and not re.fullmatch('[0-9a-f]{64}', request.get('installedBuildId', '')):
        raise ContractError('installed build identity missing')
    eligible = []
    for release in releases:
        if any(release.get(key) != request[key] for key in ('product', 'clientType', 'channel', 'profile', 'architecture')):
            continue
        if release.get('state') != 'published' or release.get('compatibilityStatus') != 'verified':
            continue
        if system not in release.get('acceptedSystems', []):
            continue
        if version_tuple(request['osVersion']) < version_tuple(release['minimumOS']):
            continue
        if not re.fullmatch('[0-9a-f]{64}', release.get('buildId', '')):
            raise ContractError('verified release has no build identity')
        if request.get('installedVersion') and product_version(release['version']) <= product_version(request['installedVersion']):
            continue
        eligible.append(release)
    if not eligible:
        return None
    eligible.sort(key=lambda release: product_version(release['version']), reverse=True)
    newest = eligible[0]
    if any(product_version(item['version']) == product_version(newest['version']) and item['buildId'] != newest['buildId'] for item in eligible[1:]):
        raise ContractError('ambiguous build for the same product version')
    # Server computes per-install deterministic rollout; a withheld new
    # version does not silently select an older rollout cohort.
    return newest if newest.get('rolloutEligible', False) else None

def validate_activation(plan, installed, system):
    """Call only AFTER signature, payload hashes, and expiry verification."""
    if plan.get('planVersion') != 2 or not re.fullmatch('[0-9a-f]{64}', plan.get('buildId', '')):
        raise ContractError('unsupported or incomplete signed plan')
    for key in ('product', 'clientType', 'profile', 'architecture'):
        if plan.get(key) != installed.get(key):
            raise ContractError('implicit migration blocked: ' + key)
    if system['nativeArchitecture'] == 'x86' and plan['architecture'] != 'x86':
        raise ContractError('wrong architecture')
    choose_profile(system['osVersion'])
    if version_tuple(system['osVersion']) < version_tuple(plan['minimumOS']):
        raise ContractError('OS below minimum')
    if plan['profile'] == 'modern' and choose_profile(system['osVersion']) != 'modern':
        raise ContractError('modern payload on legacy OS')
    if system_id(system['osVersion'], system['nativeArchitecture']) not in plan.get('acceptedSystems', []):
        raise ContractError('OS not accepted for this build')
