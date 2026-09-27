import pathlib, subprocess, sys, tempfile
compiler = sys.argv[1]
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    cases = [
        ('<Text>hello</Text>', True, ''),
        ('<Input ref="name" v-model="name"/>', True, ''),
        ('<Input ref="name.invalid"/>', False, 'static identifier'),
        ('<Column><Input ref="same"/><Input ref="same"/></Column>', False, 'Duplicate ref'),
        ('<Input v-for="item in items" :key="item.id" ref="input"/>', False, 'ref inside v-for'),
        ('<DataTable @activate="edit" @delete="remove"/>', True, ''),
        ('<Button @activate="edit"/>', False, 'require DataTable'),
        ('<DataTable @delete="remove()"/>', False, 'member references'),
        ('<SettingsPage title="Preferences"><Section/><ActionBar><Button>Save</Button></ActionBar></SettingsPage>', True, ''),
        ('<ListPage><LoadingState subtitle="Loading"/></ListPage>', True, ''),
        ('<DetailPage title="Details"><Status tone="success">Online</Status></DetailPage>', True, ''),
        ('<FormRow label="Name" :error="validation"><Input /></FormRow>', True, ''),
        ('<FormGrid min-column-width="320"><FormRow label="Name"><Input/></FormRow></FormGrid>', True, ''),
        ('<FormGrid><Input/></FormGrid>', False, 'FormRow children only'),
        ('<FormGrid><FormRow v-for="item in items" :key="item.id"><Input/></FormRow></FormGrid>', False, 'direct v-for'),
        ('<FormGrid min-column-width="0"/>', False, 'finite'),
        ('<FormGrid :min-column-width="width"/>', False, 'static'),
        ('<SettingsPage><ActionBar/><ActionBar/></SettingsPage>', False, 'at most one ActionBar'),
        ('<Status tone="red"/>', False, 'Status tone'),
        ('<Content><Text>body</Text></Content>', True, ''),
        ('<Content max-width="720" align="center"><Input /></Content>', True, ''),
        ('<Content max-width="0008" />', True, ''),
        ('<Content max-width="0" />', False, 'finite positive'),
        ('<Content max-width="880px" />', False, 'logical pixels'),
        ('<Content max-width="NaN" />', False, 'finite nonnegative'),
        ('<Content max-width="9999999999999999999999999999999999999999" />', False, 'float range'),
        ('<Content align="middle" />', False, 'start, center or end'),
        ('<Content :max-width="width" />', False, 'static'),
        ('<Content :align="alignment" />', False, 'static'),
        ('<Column max-width="720" />', False, 'Unknown property'),
        ('<Column min="200" max="100" />', False, 'max must be at least min'),
        ('<Input basis="-20" />', False, 'nonnegative'),
        ('<Input grow="inf" />', False, 'finite'),
        ('<Select selectedIndex="1.5" />', False, 'integer'),
        ('<Select selectedIndex="-1" />', True, ''),
        ('<Unknown />', False, 'Unknown component'),
        ('<Input typo="x" />', False, 'Unknown property'),
        ('<Text :text="count + 1" />', False, 'member references'),
        ('<Text v-for="row in rows" />', False, ':key'),
        ('<Column><Text v-else /></Column>', False, 'immediately follow'),
        ('<Text @click="go" />', False, 'click only'),
        ('<Input><Text>ignored</Text></Input>', False, 'Leaf components'),
        ('<FormRow />', False, 'exactly one child'),
        ('<Text>{{ count }}</Text>', False, ':text'),
        ('<Input disabled="yes" />', False, 'true or false'),
        ('<Button :text="saveText" @click="save" />', True, ''),
        ('<Column><Text v-if="visible" /><Text v-else /></Column>', True, ''),
        ('<Text v-for="item in items" :key="item.id" :text="item.text" />', True, ''),
    ]
    for index, (body, valid, diagnostic) in enumerate(cases):
        source = root / f'case{index}.one'
        source.write_text(f'<template view-model="VM">\n{body}\n</template>', encoding='utf-8')
        result = subprocess.run([compiler, '--input', str(source), '--output', str(root/'out.h'), '--name', 'Test'], capture_output=True, text=True)
        assert (result.returncode == 0) == valid, (body, result.stderr)
        if not valid:
            assert diagnostic in result.stderr, result.stderr
            assert '.one:2:' in result.stderr, result.stderr
        else:
            assert '#line 2' in (root/'out.h').read_text(encoding='utf-8')
            assert 'ui.locate(' in (root/'out.h').read_text(encoding='utf-8')
    (root/'Child.one').write_text('<template><Section><slot name="actions"/><slot/></Section></template><style scoped>Section { gap:8px; }</style>')
    (root/'Main.one').write_text('<import name="Child" src="Child.one"/><template><Child><Button slot="actions">Save</Button><Text>Body</Text></Child></template>')
    result = subprocess.run([compiler,'--input',str(root/'Main.one'),'--output',str(root/'main.h'),'--name','Main','--depfile',str(root/'main.d')],capture_output=True,text=True)
    assert result.returncode == 0, result.stderr
    generated = (root/'main.h').read_text()
    assert 'scope_Main_Child' in generated and 'slots' in generated and 'Child.one' in (root/'main.d').read_text()
    if len(sys.argv) > 3:
        cpp_compiler, includes = sys.argv[2:4]
        for body, valid in [('<Content max-width="0008" align="center"><Input v-model="name"/></Content>',True),('<Input v-model="name"/>',True),('<Input v-model="flag"/>',False),('<Text :text="flag"/>',False),('<Text :text="missing"/>',False)]:
            (root/'Typed.one').write_text('<template view-model="VM">\n'+body+'\n</template>')
            subprocess.run([compiler,'--input',str(root/'Typed.one'),'--output',str(root/'typed.h'),'--name','Typed'],check=True)
            (root/'typed.cpp').write_text('#include "oneui/ui_declarative.h"\nstruct VM { oneui::State<std::wstring> name; oneui::State<bool> flag; };\n#include "typed.h"\nvoid check(VM& vm,oneui::ui::Mount& ui){build_Typed(vm,ui);}')
            result = subprocess.run([cpp_compiler,'/nologo','/Zs','/EHsc','/std:c++17','/utf-8','/I'+includes,str(root/'typed.cpp')],capture_output=True)
            assert (result.returncode==0)==valid, result.stdout.decode(errors='replace')
            if not valid: assert b'Typed.one(2)' in result.stdout, result.stdout.decode(errors='replace')
print('Compiler positive, negative, source mapping, imports, slots and scoped CSS tests passed')
