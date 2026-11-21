# Declarative mount and container replay cost

The November 2026 iShell responsiveness investigation found two independent
sources of repeated work in the existing native views runtime. These changes
retain the current widget tree, conditional visibility, callbacks and CSS
semantics; they introduce no new view directive or deferred empty first paint.

## Owner records

`Runtime::install` previously scanned every PROP, BIND, COND and EVNT record
for every instantiated widget and imported call. Large views therefore repeated
whole-image scans, including for nodes with no bindings or events.

`OwnerRecords` builds a compact index once per Runtime image. Each section has
contiguous owner offsets and record indices. Iteration preserves the original
order, including imported property overrides, conditions and event replacement.
The storage is approximately `16 * (node_count + 1) + 4 * record_count` bytes,
excluding vector headers, where record_count is the sum of the four sections.
Construction is linear in nodes and records; installation visits only the
owner's records. No per-widget map or record copy is retained.

Reload candidates build their own index. A successful transactional reload moves
the image and its index together; failed candidates leave the old pair intact.
Indices contain offsets rather than pointers into movable image storage. The
runtime's in-memory reload tests also edit record arrays without regenerating
serialized NODE property ranges, so the index intentionally derives ownership
from the records themselves rather than trusting those old offsets.

## Container queries and local themes

`StyleSheet::forContainers` filters rules while retaining their already compiled
selectors, specificity and source order. It no longer calls `addRule` to parse
every eligible selector again. Rule mutation still uses copy on write; tokens,
transient references and derived resolution caches remain isolated as before.

Each registered query container retains its effective sheet while both the
source version and query match vector are unchanged. A base-style replay or
tree change can still require applying styles, but does not require rebuilding
the same effective rule set and losing its declaration/layout caches. New
classes and children resolve against that same immutable selected rule set.
Replacing CSS, changing tokens or crossing a query boundary rebuilds it.
Rebuild C++ consumers with the matching headers and SDK; no C ABI structure or
function was changed. The consumer Release was rebuilt against this exact DLL.

Local theme replay builds a temporary widget-to-entry lookup once, preserving
the first matching registration, nested ThemeScope inheritance and NativeHost
isolation. It avoids scanning all registered siblings for every descendant.

## Validation and measurement boundaries

Release contracts pass for container query projection, container runtime,
style scope and parse caches, plus view runtime, lists, reload and C++ parity.
They cover query boundaries and ordering, token/copy isolation, same-size base
replay, VM notifications, retained input/selection and repeated transactional
reload. These are behavior checks, not machine-specific timing thresholds.

Actual consumer measurements, matching EXE/DLL hashes and raw evidence are in
iShell's `docs/native-migration/responsiveness-3x-4x.md` and accompanying JSON.
The first query/local-theme batch alone did **not** improve its cold Settings
click (about 149 ms in both groups). The app's eager plugin/shortcut preparation
and runtime owner indexing were investigated separately after that result.
The final combined consumer build reduced a five-pair cold Settings median from
134.87 to 104.75 ms, with stable content from 158.97 to 124.98 ms. Its fixed
ten-terminal plain-text workload (900,000 lines / 72,900,000 payload bytes) fell
from 8.20 to 4.91 seconds and 19.72 to 5.95 client CPU seconds. The terminal
parser was optimized in the app as well, so these combined figures must not be
attributed solely to this SDK change.

CPU sampling that suspends threads is diagnostic only. It cannot be used as an
input-latency benchmark. Private-function attribution from a relinked MAP file
was qualified by matching export/unwind address ranges; the DLLs were not byte
identical. Module bases must be derived separately for each process run.

## Reuse the compiled base theme

View mounts validate the external theme before appending the scoped stylesheet
on a new line. The thread-local CSS cache can seed that combined parse with the
longest matching complete stylesheet. The parser compiles only the remaining
rules. Keeping the original source preserves error line and column numbers.
Rule and token mutations detach from the cached base. The result retains the
existing cascade order and the caller's transient stylesheet.

The workspace style regression compares a cached prefix with a complete parse,
including scoped overrides, late token definitions and invalid CSS rollback.
Use the view mount trace for phase times and an independent foreground capture
for the first visible response and the settled Settings canvas.

## Selector keys and widget defaults

Style resolution borrows the existing selector class strings when forming the
canonical cache key. Common selectors fit in inline storage. The ordered key
continues to distinguish semantic classes, pseudo-states and component tags.
Long class lists use the same normalization. Paint and layout caches retain
their existing bounds and rule/token invalidation.

New labels and buttons built against the declarative factory's empty Builder
keep their constructor defaults until the authored stylesheet is applied.
Replaying the same label font metrics is a no-op. A label foreground change
invalidates its paint bounds while size and weight changes still invalidate
measurement.
