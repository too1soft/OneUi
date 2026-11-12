# Atomic f64 ranges for declarative charts

`TimeSeriesChart` ranges are data coordinates. They are not layout dimensions.
The native widget stores double endpoints, but the generic declarative scalar
path previously sent them through the nonnegative float layout setter. Negative
minimums were rejected, and large close endpoints lost precision. Updating a
minimum and maximum independently could also temporarily reverse the range;
the widget then reset its endpoints to 0 and 1 before the second write.

`src/views/chart_range.cpp` handles dynamic `range-min` / `range-max` bindings
before the generic scalar path. On either endpoint notification, it reads both
currently bound endpoints from their model scopes, using the existing runtime
observation mechanism. It validates a finite `minimum < maximum` pair, then
calls the native chart's double setter once. No float conversion, clamping,
sorting, offset or normalization is applied to the data.

An unbound counterpart uses the native widget's current endpoint. Applications
should bind both endpoints when replacing a complete data range; update both
model fields and notify each changed property before flushing. The runtime's
existing model cache contract remains: changing a field without notifying it
does not publish that change. A bad pair returns `ONEUI_VIEW_TYPE` without
changing the native range. A later valid notification can recover. The C ABI,
IR property IDs, schema, compiler contract and public native chart APIs do not
change. Existing frozen views and static authored ranges keep their contracts.

This repair concerns dynamic model bindings. It does not redefine the generic
float authoring grammar, all layout numeric validation, or arbitrary static
chart attributes. It introduces no scripts, networking, timers or permissions.

The runtime's `apply` method only dispatches to the chart-specific handler.
Range policy lives in the separate handler rather than spreading into layout
validation. Both bound fields remain observed; changing just one endpoint
requires only its notification and retains the unchanged counterpart. A closed
scope is never read.

`tests/views/chart_contract.inc` exercises real IR, record adaptation and the
native chart. It covers positive ranges disjoint from the initial 0–100,
negative-only and mixed ranges, large narrow positive and negative f64 ranges,
notifications in either endpoint order, equal/reversed/nonfinite rejection,
preservation of the previous valid native range, and recovery. The shared
Runtime fixture initializes the maximum to 100; other fixture behavior remains.

The triggering client is iShell's bounded plugin chart attachment. Its real
hidden-window probe originally failed the first data flush with
`Component property value rejected (range-min)`. The probe uses the actual
workbench VM and generated view. Core SDK, HTML simulation, native attachment,
ordinary visible-window interaction and platform delivery remain separate
evidence layers.

On this machine, use the fixed C-drive build directory
`C:/Users/Administrator/.codex/builds/ishell_oneui/oneui-plugin-charts`, a Visual
Studio amd64 developer environment, and the explicit Python interpreter for
configuration. Build `oneui` and `oneui_view_runtime_tests`; run the latter with
that build's `Runtime.oneview`. The pre-existing copied `editor-capture` cache
has no rules.ninja and is not a usable build. A new complete C-drive build
avoids writing intermediate objects to the nearly full E drive.

The client-side source path under its isolated build is a junction to the
actual OneUI checkout. It is not a separate OneUI source snapshot. Existing
unrelated OneUI changes are preserved byte-for-byte, and the DLL build uses the
actual current framework checkout. Do not describe this as compiling only the
client's isolated source or as acceptance of unrelated framework changes.

Actual build results, artifact hashes and client combination results are
recorded in the client repository's `docs/plugins/charts-validation.md` and
`qa-evidence/plugin-charts` under the fixed build root. Source changes alone
must not be reported as a loaded or accepted native client.
