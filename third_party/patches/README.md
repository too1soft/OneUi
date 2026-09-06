# Reviewed dependency patches

## ICU 74.2 — Unicode 15.1 Bidi conformance

`icu-74.2-bidi-unicode-15.1.patch` is authored and maintained by OneUI, **not an
upstream backport**. It modifies ICU's private implementation only. ICU's public
API and OneUI's C ABI are unchanged. Retain the source's Unicode license notices;
the patch is provided under those same terms.

The Skia base stays at `1f26101197bff9fcd939a791beb3094297436d59`, and its DEPS ICU
base stays at `364118a1d9da24bb5b770ac3d762ac144d6da5a4` (74.2 / Unicode 15.1).
The patch SHA-256 (LF):
`8a2e7fcbd8c319bffb3e1d4acb55ff5611f0aea57cc5c810f4e6236ce4dacf0a`.

Corrections follow [Unicode 15.1 UAX #9, revision 49](https://www.unicode.org/reports/tr9/tr9-49.html):

- X5a/X5b/X6a: preserve directional overrides for isolate initiators and PDI.
- X6/BD14–BD16: overridden brackets are strong L/R, not pairing candidates.
- N0: propagate the final bracket direction to following original NSMs after
  pending N0c context corrections, without changing W2/W7 strong-type state.
- BD16: enforce the 63-opening stack per isolating run sequence. Count canonical
  synonyms once and exclude closed N0c entries. Overflow returns an empty pair
  list for the entire affected sequence, including pairs previously resolved
  inline; nested isolates and subsequent level runs/paragraphs remain independent.

The full unmodified Unicode fixtures reproduce 30 Bidi failures without this
patch and pass with it. Additional capacity, canonical-equivalence, rollback,
isolate, paragraph and pending-N0c regressions run in
`tests/unicode_conformance_tests.cpp`. Word segmentation retains ICU's explicitly
tested root-locale colon tailoring; this patch does not change that profile.

Windows and POSIX Skia build scripts call `scripts/apply-skia-patches.cmake` after
DEPS sync and before compilation. It verifies the base commit and LF-normalized
pre/post source SHA-256 hashes, applies only pristine inputs, accepts exactly the
already-applied state, and rejects unknown changes without overwriting them.
OneUI configuration checks read-only and fails if the patch is absent.

Always rebuild `skia skparagraph skunicode_icu`, not just `icu`: GN's complete
`skunicode_icu` archive contains ICU objects too. A source check alone cannot
certify arbitrary externally supplied prebuilt archives; run the conformance
target against the final linked artifacts. The normal SDK carries the resulting
library, not an extra ICU runtime or data file.

When updating the dependency pin, reassess/remove the patch against the new
upstream implementation and rerun every conformance case. Never silently apply
this patch to another revision or label missing native-platform acceptance as
completed. Current platform and release status is in
[the text-engine contract](../../docs/38-text-and-interaction-engine.md).
