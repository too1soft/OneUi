"""Rust emitter contract tests; actual rustc type errors live in oneui-view-tests."""
import unittest
from support import CompilerCase


class Rust(CompilerCase):
    def emit(self, schema, error=None):
        self.write('Basic.one', '<template view-model="Basic"><Text text="test"/></template>')
        self.write('Basic.oneschema', schema)
        self.run_cli('--input', self.root / 'Basic.one', '--schema', self.root / 'Basic.oneschema',
                     '--emit', 'rust', '--output', self.root / 'generated.rs',
                     '--ir-output', self.root / 'Basic.oneview', error=error)
        if error is None:
            self.run_cli('--verify', self.root / 'Basic.oneview')
            return (self.root / 'generated.rs').read_text(encoding='utf-8')

    def test_readonly_and_commands(self):
        text = self.emit('model Basic { r title: utf8 rw enabled: bool command choose(value: i64) }')
        self.assertIn('fn r#title(&self) -> &str', text)
        self.assertNotIn('fn set_title', text)
        self.assertIn('fn set_enabled', text)
        self.assertIn('Choose { r#value: i64', text)
        self.assertIn('oneui::view::define_view_adapter!', text)

    def test_dotted_property_collision(self):
        self.emit('model Basic { r a.b: utf8 r a_b: utf8 }', error='RUST_NAME')

    def test_command_variant_collision(self):
        self.emit('model Basic { command do_it() command do.It() }', error='RUST_NAME')

    def test_keywords(self):
        text = self.emit('model Basic { rw type: utf8 r self: bool command match() }')
        self.assertIn('fn r#type', text)
        self.assertIn('fn r#field_self', text)


if __name__ == '__main__':
    unittest.main()
