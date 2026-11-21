# Typed CSS validation on view mounts

`oneui_view_mount_set_styles` validates the typed CSS dialect before the regular
style parser's cache is consulted. Repeatedly mounting the same application
theme therefore needs a separate fast path here.

The validator reuses immutable selector/numeric regular expressions and native
property sets. Successful rules no longer copy the unprocessed suffix or count
all preceding newlines; diagnostic line counting happens when it is needed.
Accepted properties, selector mapping and existing diagnostics remain the same.

Successful normalized results are remembered in a thread-local cache:

- The key includes comment-normalized source, scope, container-query context
  and the complete set of external component names.
- File name and starting line affect diagnostics only. Invalid input is never
  cached, and unclosed comments are checked before a lookup.
- At most eight entries and 1 MiB of string payload are retained per thread.
  Large results are not retained. Allocation failure while remembering a result
  leaves the successful validation usable.
- Results are returned as independent strings. No widgets, resolved theme
  tokens or mutable state from another mount enter this cache.

Build the focused contracts and the optional standalone CPU benchmark in the
existing build directory:

```text
ninja -C build/release oneui_css_validation_tests oneui_validated_css_cache_tests
ctest --test-dir build/release -R "oneui_(css_validation|validated_css_cache)_tests" --output-on-failure
ninja -C build/release oneui_css_validation_benchmark
build/release/oneui_css_validation_benchmark theme.css normalized.css
```

The benchmark reports the first call separately from fourteen identical calls.
It creates no window. Compare normalized output as well as elapsed time; it
does not measure page mounting, input latency or display presentation.

`oneui_css_validation_tests --corpus` emits a deterministic corpus of native
component properties and rejected dialect cases. Compile that same source with
the previous header first in the include path to compare outputs byte for byte.
The contracts also check scope/external-component/query isolation and fresh
diagnostic locations. The cache tests cover both limits and thread ownership.
