# Optional Android document access

Compile `java/net/too1/oneui/documents/*.java` in the Android consumer's source set. This optional module uses Android's `ContentResolver`; it does not require OneUI views or native rendering. Pass a tree URI and the actual flags returned by the system document picker to `select`. Grant checks and provider calls belong to the app's background worker.

`request(String)` retains the existing JSON operations and seconds-based `modified` field. `request(String, CancellationSignal)` propagates cancellation to queries and read opens. Metadata additionally includes `modifiedMillis`; `list` optionally accepts a positive `limit`, returning an error if the folder exceeds it. Name lookup scans a cursor without creating a JSON object for every sibling. Existing unbounded list callers keep their behavior.

The version 2 `capabilities` response advertises read cancellation, bounded listing, precise modification times and source roots. Read-only sources use the following local operations:

| Operation | Inputs | Result |
| --- | --- | --- |
| `source-root` | selected virtual `path` | private `source` token and root `entry` |
| `source-stat` | `source`, relative `path` (empty for root) | metadata or null |
| `source-list` | `source`, relative `path`, positive `limit` | bounded metadata array |
| `source-open` | `source`, relative file `path` | detached `fd`, retained `descriptor` token and metadata |
| `close` | retained `descriptor`, optional error | closes the provider's error channel |
| `source-close` | `source` | releases the root token |

Root tokens are process-local and bounded to 32 per `DocumentAccess`. They retain a provider document URI instead of looking up the selected root's old parent/name again. Each operation rechecks the tree grant. Identity remains subject to the provider's document-ID semantics; a URI is not an OS directory handle, and a provider that reuses IDs is not an immutable snapshot.

The caller owns the detached FD, must close it, then call `close` to observe provider errors. On cancellation, close active read descriptors and cancel provider queries; call `source-close` after workers finish. Closing a token prevents new requests; an operation already in progress may finish. Tokens, descriptor IDs and provider URIs must stay within the local adapter. Remote peers must never choose or receive them.

Use `scripts/test-android-document-sources.py` to build and run the contract fixture. The stage requires `.owner` containing `oneui-android-document-test`; each attempt gets its own directory. The fixture uses Android `ContentResolver` and a controlled provider under the same UID. It does not verify the system picker, external providers, persistent URI grants, a remote engine or a product UI.
