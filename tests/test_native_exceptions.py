import unittest
from compatlib import canonical, ContractError, validate_native_exception


class NativeExceptions(unittest.TestCase):
    def test_exact_scope_only(self):
        manifest = dict(product='wyc',clientType='gui',version='2.4.3',profile='legacy',architecture='x86_64',buildId='a'*64)
        record = dict(exceptionId='owner-x64-20260909',systemId='win7-sp1-original-x86_64')
        grant = dict(manifest, id=record['exceptionId'],systems=[record['systemId']],checks=['native','business'],
                     approvedBy='product-owner',authorization='2026-09-09 user: Windows x64 native testing deferred',reason='Owner will test after launch')
        exceptions=dict(schemaVersion=1,exceptions=[grant])
        validate_native_exception(manifest,'native',record,exceptions)
        for check in ('imports','build','trust'):
            with self.assertRaises(ContractError): validate_native_exception(manifest,check,record,exceptions)
        for field,value in [('version','2.4.4'),('profile','modern'),('architecture','x86'),('buildId','b'*64)]:
            with self.assertRaises(ContractError): validate_native_exception(dict(manifest,**{field:value}),'native',record,exceptions)
        with self.assertRaises(ContractError): validate_native_exception(manifest,'native',record,None)

    def test_canonical_unicode_and_integer_policy(self):
        self.assertEqual(canonical({'\ue000':1,'\U0001f600':2,'text':'中文<>&\u2028'}),
            '{"text":"中文<>&\u2028","😀":2,"":1}'.encode())
        for invalid in (float('nan'),1.1,2**53,{'key':'\ud800'}):
            with self.assertRaises((ContractError,UnicodeError)): canonical(invalid)


if __name__ == '__main__': unittest.main()
