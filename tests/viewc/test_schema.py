from support import *


class SchemaTests(CompilerCase):
    def test_order_and_auto_ids(self):
        a = self.compile(schema='model Basic { r z: bool r title: utf8 command z() command a() }')
        b = self.compile(schema='// reorder\nmodel Basic { command a(); r title:utf8; command z(); r z:bool; }')
        self.assertEqual(a, b)
        schema = section(a, 2)
        self.assertIn(struct.pack('<I', 1) + struct.pack('<I', 5) + b'title', schema)

    def test_full_reachable_hash(self):
        base = 'model Basic { r title:utf8 r items:list<Item> r can:bool r busy:bool command act(x:i64) when can busy busy } model Item key id { r id:utf8 r value:i64 }'
        original = self.compile(schema=base)[48:80]
        changes = [
            ('r value:i64', 'r value:f64'), ('r value:i64', 'rw value:i64'),
            ('r value:i64', 'r renamed:i64'), ('r value:i64', 'r value:i64 r extra:bool'),
            ('act(x:i64)', 'act(x:f64)'), ('act(x:i64)', 'act(y:i64)'),
            ('act(x:i64)', 'act(x:i64,y:utf8)'), ('when can ', ''), ('busy busy', ''),
            ('key id', 'key alternate'), ('r id:utf8', 'r id:utf8 r alternate:utf8'),
        ]
        for old, new in changes:
            with self.subTest(change=(old, new)):
                schema = base.replace(old, new)
                if new == 'key alternate':
                    schema = schema.replace('r id:utf8', 'r id:utf8 r alternate:utf8')
                self.assertNotEqual(original, self.compile(schema=schema)[48:80])

    def test_unused_roots_pruned_but_reachable_fields_hashed(self):
        first = self.compile()
        self.assertEqual(first, self.compile(schema='model Other { r x:bool } model Basic { r title:utf8 }'))
        self.assertNotEqual(first[48:80], self.compile(schema='model Basic { r title:utf8 r unused:bool }')[48:80])
        self.assertEqual(first[48:80], self.compile('<Text text="different"/>')[48:80])

    def test_import_diamond_and_depfile(self):
        self.write('shared.oneschema', 'model Item key id { r id:utf8 }')
        self.write('a.oneschema', 'import "shared.oneschema"')
        self.write('b.oneschema', 'import "shared.oneschema"')
        self.compile(schema='import "a.oneschema" import "b.oneschema" model Basic { r title:utf8 r items:list<Item> }')
        dep = (self.root / 'Basic.d').read_text()
        self.assertEqual(dep.count('shared.oneschema'), 1)
        self.assertIn('a.oneschema', dep)
        self.assertIn('b.oneschema', dep)

    def test_parser_diagnostics(self):
        cases = [
            ('import "missing.oneschema" model Basic {}', 'SCHEMA_IO'),
            ('model Basic { r title:wat }', 'SCHEMA_TYPE'),
            ('model Basic {r title:utf8} model Unused {command bad() when missing}', 'SCHEMA_GATE'),
            ('model Basic { r title:utf8 r title:bool }', 'SCHEMA_DUPLICATE'),
            ('model Basic { command a() command a() }', 'SCHEMA_DUPLICATE'),
            ('model Basic {} model Basic {}', 'SCHEMA_DUPLICATE'),
            ('model Basic { r a: list<Missing> }', 'SCHEMA_TYPE'),
            ('model Basic { rw a:list<Item> } model Item key id {r id:utf8}', 'SCHEMA_LIST'),
            ('model Basic { r a:list<Item> } model Item {r id:utf8}', 'SCHEMA_KEY'),
            ('model Basic key id { rw id:utf8 }', 'SCHEMA_KEY'),
            ('model Basic key id { r id:i64 }', 'SCHEMA_KEY'),
            ('model Basic key id { r id:utf8 r next:list<Basic> }', 'SCHEMA_CYCLE'),
            ('model Basic { command a(x:list<Item>) }', 'SCHEMA_COMMAND'),
            ('model Basic { command a(x:i64,x:i64) }', 'SCHEMA_COMMAND'),
            ('model Basic { rw can:bool command a() when can }', 'SCHEMA_GATE'),
            ('model Basic { r busy:utf8 command a() busy busy }', 'SCHEMA_GATE'),
            ('model Basic { r title:utf8', 'SCHEMA_SYNTAX'),
            ('model Basic { r title:utf8 @ }', 'SCHEMA_SYNTAX'),
            ('model Basic { r title:utf8 = 42 }', 'SCHEMA_SYNTAX'),
            ('model Basic { r bad..name:utf8 }', 'SCHEMA_SYNTAX'),
            ('import "https://example/schema" model Basic {}', 'SCHEMA_IMPORT'),
        ]
        for schema, code in cases:
            with self.subTest(code=code, schema=schema):
                self.compile(schema=schema, error=code)

    def test_import_cycle(self):
        self.write('cycle.oneschema', 'import "Basic.oneschema"')
        self.compile(schema='import "cycle.oneschema" model Basic {}', error='SCHEMA_CYCLE')

    def test_utf8_diagnostic(self):
        self.compile(schema=b'model Basic {\xff}', error='SCHEMA_UTF8')

    def test_root_and_line_column(self):
        result = self.compile(schema='model Basic {\n  r title: nope\n}', error='SCHEMA_TYPE')
        self.assertIn('Basic.oneschema:2:12:', result.stderr)
        self.compile('<template><Text/></template>', error='SCHEMA_ROOT', raw=True)


if __name__ == '__main__':
    unittest.main()
