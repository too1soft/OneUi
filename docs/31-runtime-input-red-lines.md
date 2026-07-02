# OneUI Runtime/Input Red Lines

Date: 2026-05-29

OneUI owns the visual and input primitives. Downstream products own business runtime wiring. The boundary must stay explicit.

## Red Lines

1. `RemoteInputRegion` and similar primitives must not dispatch pointer coordinates until their content geometry is valid. Unknown remote size is a not-ready state, not `(0, 0)`.
2. Downstream products must not compensate for missing primitive readiness by filtering magic coordinates or component names.
3. GUI-to-runtime communication inside one process should use typed commands/channels. A localhost TCP bridge is only justified when crossing a real process boundary.
4. Capture/render/input loops must be independently scheduled. A blocking video loop must never starve input receive or injection.
5. E2E must assert controlled-side delivery, not only sender-side enqueue. For remote control this means checking host-side `input reached` or `input injected` logs.
6. JSONL telemetry written from multiple threads must be serialized or otherwise made line-safe before tests consume it.
7. Same-desktop loopback tests must not mistake keyboard injection feedback for product behavior. The current Remote loopback uses a low-level keyboard hook and requires host-side injected virtual-key down/up events (`LLKHF_INJECTED`) after the controller emits exactly one down/up pair. Do not weaken this to sender-side telemetry only. Use a separate desktop/VM E2E when the assertion must prove target-application text input.
8. `RemoteInputRegion` must release all pressed pointer/key state on focus loss. Missing key-up after focus transitions is a framework bug and can leave downstream remote-control products with stuck keys.
9. Streaming JSONL readers may tolerate a final incomplete line from a live writer and must use shared reads for active telemetry files, but invalid completed records must stay fatal.
10. Real remote-control readiness requires a separate-desktop/VM target-application assertion. Framework primitives can expose input telemetry, but downstream products must also prove that the controlled-side focused application state changes through the visible session window.
11. Performance work must include instrumentation. For remote-control products, collect per-frame render/input metrics such as capture, encode, transport wait, decode, color conversion, latency, FPS, bitrate, and p95 frame interval before claiming an optimization worked.

These rules are generic framework rules. They apply to Remote, WYC, iShell, and any future OneUI downstream product.
