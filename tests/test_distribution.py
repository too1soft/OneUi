import copy
import unittest
from compatlib import ContractError
from distribution import select_release, validate_activation, product_version

class Distribution(unittest.TestCase):
    def setUp(self):
        self.request = dict(product='wyc', clientType='gui', channel='stable', profile='legacy', architecture='x86',
            nativeArchitecture='x86', osVersion='6.1.7601', capabilities=['windows_compat_v2'],
            installedVersion='2.4.1', installedBuildId='a'*64)
        self.release = dict(product='wyc', clientType='gui', channel='stable', profile='legacy', architecture='x86',
            minimumOS='6.1.7601', acceptedSystems=['win7-sp1-original-x86'], state='published', compatibilityStatus='verified',
            version='2.4.2', buildId='b'*64, rolloutEligible=True)
    def test_filter_before_latest(self):
        modern = dict(self.release, profile='modern', version='2.4.9')
        self.assertEqual(select_release([modern,self.release],self.request)['version'],'2.4.2')
    def test_dimensions_do_not_mix(self):
        for key,new in [('profile','modern'),('architecture','x86_64'),('channel','beta'),('clientType','service'),('product','other')]:
            self.assertIsNone(select_release([dict(self.release, **{key:new})],self.request))
    def test_historical_default_is_not_evidence(self):
        self.assertIsNone(select_release([dict(self.release,compatibilityStatus='unknown')],self.request))
    def test_os_not_assumed_from_another_test(self):
        self.assertIsNone(select_release([self.release],dict(self.request,osVersion='6.3.9600')))
    def test_missing_or_old_identity(self):
        for changes in [dict(capabilities=[]),dict(installedBuildId=''),dict(osVersion='unknown'),dict(nativeArchitecture='arm64')]:
            with self.assertRaises(ContractError): select_release([self.release],dict(self.request,**changes))
    def test_installed_legacy_stays_legacy_on_modern_os(self):
        release = dict(self.release,acceptedSystems=['win10-x86_64'])
        request = dict(self.request,osVersion='10.0.19045',nativeArchitecture='x86_64')
        self.assertEqual(select_release([release],request)['profile'],'legacy')
    def test_same_version_never_replaces_build(self):
        self.assertIsNone(select_release([dict(self.release,version='2.4.1')],self.request))
        with self.assertRaises(ContractError): select_release([self.release,dict(self.release,buildId='c'*64)],self.request)
    def test_rollout_after_compatibility(self):
        self.assertIsNone(select_release([dict(self.release,rolloutEligible=False)],self.request))
    def test_semver(self):
        self.assertLess(product_version('2.4.2-rc.2'),product_version('2.4.2-rc.10'))
        self.assertLess(product_version('2.4.2-rc.10'),product_version('2.4.2'))
    def test_local_activation_blocks_implicit_migration(self):
        plan=dict(self.release,planVersion=2)
        validate_activation(plan,self.request,self.request)
        for changes in [dict(architecture='x86_64'),dict(profile='modern'),dict(buildId=''),dict(acceptedSystems=[])]:
            with self.assertRaises(ContractError): validate_activation(dict(plan,**changes),self.request,self.request)

if __name__ == '__main__': unittest.main()
