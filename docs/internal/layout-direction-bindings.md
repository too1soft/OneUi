# Declarative native layout direction

`Row` and `Column` accept optional `direction="row|column"`, as literal or read-only UTF-8 binding. This permits one native Stack and one set of child view models to change layout direction. Rendering hidden Row and Column copies attaches the same child model twice and can overwrite its binding context; direction avoids that ownership error.

The iShell plugin composition integration is the initial caller. Its manifest limits layout to 64 unique nodes and four levels; those are application constraints, not new OneUI limits. OneUI still rejects recursive schema models. The application explicitly unrolls its bounded model/template chain.

## Behavior

- Omitting the property preserves Row's horizontal and Column's vertical defaults.
- `row` calls existing `Stack::setDirection(StackDirection::Row)`; `column` selects Column.
- The control, child widgets, focus ownership and model identities remain the same.
- Existing wrap, align, justify and layout properties continue to apply to that Stack.
- Values are exact lower-case tokens. Empty, differently cased and unknown tokens fail before changing native direction. A later valid notification can recover.
- The property is restricted to Row and Column. It is not a global arbitrary widget property or CSS selector.
- Changing layout does not execute an application command, reload data or create native resources.

## Serialization and compatibility

Authoring metadata adds `direction` in `include/oneui/ui_component_schema.h`. The serialized registry appends **property ID 14**, UTF-8 tag 4, flags 3 (literal and binding), to each component. Row moves from v2 to v3; Column moves from v3 to v4. Existing IDs and fields retain their serialized order. No C ABI change is involved.

The preceding exact descriptors at `f73dfd79e63961bc4171e43170072099e4ccf2ff` are frozen in `src/views/format/legacy_layout_contract.h`. Column's previous bound basis/class flags and Row's previous class flags are explicitly recorded. Historical contracts must not be synthesized by deleting the new field from the current registry.

`legacy_component_contract.h` includes those descriptors in its complete historical list. The loader compares every version, field ID/name/tag/flags/record, event and child/model rule before accepting historical input. Normalization follows full validation. Merely presenting an old version does not authorize altered flags or the new property ID.

A compiler with the new authoring header but an old serialized registry fails its registry drift checks. A new compiler with an old runtime cannot reliably load the resulting IR. Build and distribute a matching compiler/runtime pair; updating only an application template is insufficient.

## Sources and validation

| File | Responsibility |
| --- | --- |
| `include/oneui/ui_component_schema.h` | Authoring property admission. |
| `src/views/format/component_registry_layout.inc` | Appended stable property IDs. |
| `src/views/format/component_contract.cpp` | Explicit component version increases. |
| `src/views/format/legacy_layout_contract.h` | Complete pre-direction serialized descriptors. |
| `src/views/format/legacy_component_contract.h` | Exact historical decoder integration. |
| `src/views/component_properties.cpp` | Validate tokens, then dispatch existing native setter. |
| `tests/views/Runtime.one`, `Runtime.oneschema`, `fixture.h` | Real IR and initial UTF-8 direction value. |
| `tests/views/layout_direction_contract.inc` | Bound reflow retaining child identity, invalid-value retention/recovery, exact historical descriptor round-trip and forged-flag rejection. |

The existing Runtime driver also consumes historical dependent fixtures. Before the exact preceding layout descriptors were added, that driver failed `VERSION: component contract`; this was a real compatibility failure, not an acceptable fixture rewrite. Both the Runtime and frozen legacy-contract drivers pass after the decoder fix. Application Native tests additionally attach a real four-level composition and retain editable text, form drafts, selected result objects and disabled actions.

Use a fixed build directory and explicitly matched DLL when reproducing. On the current Windows CMake/Ninja cache, Chinese compiler include prefixes did not reliably rebuild some header/include dependents. The changed component-contract translation units and affected test driver were explicitly rebuilt. This is a local freshness measure; global dependency scanning has not been repaired.

These tests establish source/IR/runtime behavior. A hidden application attachment is independent from ordinary visible-window appearance, platform-specific input and accessibility verification. No macOS/Linux or ordinary application-window visual result is inferred here.
