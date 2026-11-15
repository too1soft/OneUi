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

## Resumable receive transactions

Version 2 additionally advertises `receiveTransactions: 1`. Receiving requires both read and write permissions from the actual picker result. The module rechecks those permissions for each receive operation. A receive root pins a provider URI and uses a stable scope derived from the granted tree and selected document URI. Renaming/replacing the selected folder's parent entry does not redirect an existing root. A new selection with a different document URI has a different scope.

| Operation | Inputs | Result |
| --- | --- | --- |
| `receive-root` | selected virtual `path` | process-local `root`, stable `scope`, root `entry` |
| `receive-stat` | `root`, relative `path` (empty for root) | metadata or null |
| `receive-list` | `root`, relative directory `path`, positive `limit` | bounded metadata array |
| `receive-mkdir` | `root`, relative directory `path` | true |
| `receive-open` | `root`, local `key`, relative file `path`, `overwrite`, `expected` | receiving FD/token/metadata, or published receipt |
| `receive-finish` | `root`, `key` | published entry and `cleanupPending` |
| `receive-recover` | `root`, `key` | recovered stage, or an error preserving uncertain copies |
| `receive-discard` | `root`, `key` | removes the owned temporary; preserves published/uncertain copies |
| `receive-forget` | `root`, `key` | removes a published receipt after verified backup cleanup |
| `receive-root-close` | `root` | releases the root token; resumable state remains |

`expected` is the complete metadata returned by `receive-stat`, or explicit JSON null when the target must be absent. Replacing a file also requires `overwrite: true`; directories cannot be overwritten. The snapshot compares provider URI, type, size and millisecond modification time. It is a stale-target check, not an atomic provider compare-and-swap or a content digest. Consumers must still verify the actual prefix, offsets, final size and digest through their existing transfer/checkpoint algorithm. The SDK does not decide whether a network transfer is complete.

The local adapter must namespace a stable `key` (1–128 ASCII letters, digits, underscores or hyphens) for one target. Reopening that key cannot change its target. A receiving result contains `fd`, retained `descriptor`, `transaction`, private `path`, `created`, and the temporary's current `entry`. Reopening uses `rw` without truncation. `created` reports an actually created receiving document; it does not prove a checkpoint. Seek, truncation and durability capabilities remain provider-dependent; consumers must reject unsupported operations rather than silently substitute app-private content storage.

The data lives in a random temporary sibling in the granted provider tree. Only ownership and recovery metadata (at most 64 KiB per transaction) are stored in private app files, using synchronized file writes and atomic replacement. No received content is copied there. The caller flushes/synchronizes and closes the detached data FD, calls `close` on the retained descriptor, checks both results, then calls `receive-finish`. Supplying an error to `close` or receiving a provider close error blocks publication. A lease remains held until this close completes. Root closure does not close active descriptors; the local adapter must close those separately.

Private kernel leases exclude another process, and an in-process guard rejects overlap before opening a second lock FD. This ordering matters on POSIX: closing a second descriptor for a locked inode can release the process's first record lock. Keep one SDK class definition and the same preferences namespace for cooperating consumers. Lease files deliberately remain after completion; unlinking one while another opener holds its inode could split the lock. They contain no file content. App owners must account for this metadata footprint; no unbounded content cache or universal garbage-collection guarantee is implied.

After a process restart, reselect/regrant the same folder, obtain a fresh root token, reopen the same key, and verify the existing provider bytes against the consumer's durable checkpoint before continuing. Process death releases the kernel lease. An interrupted writer cannot be finished without reopening and successfully closing I/O. A completed publication receipt remains until `receive-forget`, allowing the consumer to reconcile a crash between provider publication and its own checkpoint commit.

Publication first moves an existing target to an owned backup, then renames the verified temporary into place. Failures restore the original when identity evidence permits. A crash after the final rename can be recognized for opaque stable document IDs. If rename changes IDs and the outcome cannot be proved, recovery reports an error and preserves both copies. Discard moves an uncertain backup to a `Recovered-…` name; it does not delete the only recoverable original. Providers may reuse IDs or alter metadata concurrently, so these operations cannot provide a universal atomic replacement guarantee. The consumer must expose recovery failures and retain its checkpoint.

`scripts/test-android-document-receives.py` builds an independent same-UID `DocumentsProvider` fixture. The fixed stage requires `.owner` containing `wangyunkong-go-saf-receive`. Each positive/negative build and run uses a new attempt directory. Contracts cover descriptors and error channels, cancellation, stale targets, pinned roots, cross-process exclusion after local overlap attempts, normal restart, force-stop with an open writer, publication crashes with stable/changing IDs, and legacy begin/finish/discard. This fixture has no discoverable picker entry and does not test external providers, actual persisted system grants, product JNI/Go receive integration, UI, power loss, or remote-network delivery.
