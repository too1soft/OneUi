from support import *


class OverlayTests(CompilerCase):
    def test_static_binding_event_and_owner_kind(self):
        self.write('Child.one', '<template><Button class="inner" text="root"/></template>')
        data = self.compile('<import name="Child" src="Child.one"/><template view-model="Basic">'
                            '<Child class="outer" :text="title" @click="run"/></template>',
                            'model Basic { r title:utf8 command run() }', raw=True)
        self.assertEqual(directory(data)[8][5], 20)
        self.assertEqual(directory(data)[9][5], 36)
        self.assertEqual(directory(data)[10][5], 36)
        self.assertEqual(u32(section(data, 9), 32), 1)
        self.assertEqual(u32(section(data, 10), 32), 1)
        broken = bytearray(data)
        struct.pack_into('<I', broken, directory(data)[9][2]+32, 0)
        self.verify_bytes(broken, 'owner_kind')

    def test_bad_property_reports_call_site(self):
        self.write('Child.one', '<template><Text/></template>')
        result = self.compile('<import name="Child" src="Child.one"/><template view-model="Basic">\n'
                              '<Child checked="true"/></template>', error='property', raw=True)
        self.assertIn('Basic.one:2:', result.stderr)

    def test_nested_call_and_caller_scope(self):
        self.write('Leaf.one', '<template><Text text="leaf"/></template>')
        self.write('Child.one', '<import name="Leaf" src="Leaf.one"/><template><Leaf class="inner"/></template>')
        data = self.compile('<import name="Child" src="Child.one"/><template view-model="Basic">'
                            '<Column v-for="item in rows" :key="item.id"><Child :text="title"/></Column></template>',
                            'model Basic { r title:utf8 r rows:list<Item> } model Item key id {r id:utf8}', raw=True)
        record = section(data, 9)
        self.assertEqual(struct.unpack_from('<H', record, 12)[0], 1)

    def test_temp_cleanup_on_exception(self):
        with self.assertRaisesRegex(RuntimeError, 'fixture failure'):
            with temporary_workspace(SCRATCH) as path:
                directory_path = Path(path)
                (directory_path/'partial').write_text('x', encoding='utf-8')
                raise RuntimeError('fixture failure')
        self.assertFalse(directory_path.exists())


if __name__ == '__main__':
    unittest.main()
