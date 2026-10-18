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

    def test_typed_dispatch_preserves_empty_command_api(self):
        text = self.emit('model Basic { r title: utf8 }')
        self.assertIn('pub enum BasicCommand {\n}', text)
        self.assertIn('let command: Result<BasicCommand, ViewError> = match id', text)
        self.assertIn('Ok(command) => match command {}, Err(error) => Err(error)', text)
        self.assertNotIn('fn invoke(&mut self', text)
        self.assertIn('Basic_properties::TITLE => Ok(Value::Utf8(vm.r#title()', text)
        self.assertIn('Basic_properties::TITLE => Err(ViewError::READ_ONLY)', text)
        # A property named invoke remains legal for a command-free model.
        self.assertIn('fn r#invoke(&self)', self.emit('model Basic { r invoke: bool }'))

    def test_typed_command_dispatch(self):
        text = self.emit('model Basic { command choose(value: i64) }')
        self.assertIn('Ok(BasicCommand::Choose { r#value: args[0].integer()?', text)
        self.assertIn('vm.invoke(command?, cx)', text)

    def test_command_variant_collision(self):
        self.emit('model Basic { command do_it() command do.It() }', error='RUST_NAME')

    def test_keywords(self):
        text = self.emit('model Basic { rw type: utf8 r self: bool command match() }')
        self.assertIn('fn r#type', text)
        self.assertIn('fn r#field_self', text)

    def test_qualified_entities_retain_legacy_public_aliases(self):
        text = self.emit('model Basic { r items: list<oneui.v1.StringItem> }')
        self.assertIn('pub enum OneuiV1StringItemCommand {', text)
        self.assertIn('pub trait OneuiV1StringItemVm:', text)
        self.assertIn('pub use self::OneuiV1StringItemCommand as oneui_v1_StringItemCommand;', text)
        self.assertIn('pub use self::OneuiV1StringItemVm as oneui_v1_StringItemVm;', text)
        self.assertIn('type ItemsItem: oneui_v1_StringItemVm;', text)
        self.assertIn('Result<oneui_v1_StringItemCommand, ViewError>', text)
        self.assertNotIn('non_camel_case_types', text)

    def test_qualified_entity_alias_collision_is_rejected(self):
        self.emit('model Basic { r a:list<a.b> r b:list<AB> } model a.b key id { r id:utf8 } model AB key id { r id:utf8 }', error='collide')


if __name__ == '__main__':
    unittest.main()
