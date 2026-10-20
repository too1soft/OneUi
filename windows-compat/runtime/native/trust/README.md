# Process-local Authenticode candidate

Status: experimental shared C ABI; **not integrated into a shipping installer,
Agent, GUI or publication approval**. Do not upload the probe as a product.

This is distinct from OneUpdate's existing Ed25519 update-plan signatures and
SHA-256 payload verification. It does not change the Windows shell's publisher
label or repair its system certificate store.

## Policy

- Original Win7 SP1 ABI, static MSVC CRT, x86 and x64. No extra DLL distribution.
- `WINTRUST_ACTION_GENERIC_CHAIN_VERIFY` verifies the Authenticode file/SIP and
  signatures; a final callback applies Authenticode and timestamp chain policy.
- A memory-only exclusive root store contains the two public roots in
  `certificates/sources.json`. DER hashes are verified before embedding them.
- Signing EKU, timestamp EKU, SHA-256 signer, exact expected leaf certificate
  SHA-256, timestamp and revocation checks are mandatory. Only return value zero
  means success. Missing revocation information must not be treated as success.
- No `IGNORE_*` trust flags, automatic root update, system root writes, or
  fallback to an arbitrary certificate contained in the downloaded file.
- Files are held open without write/delete sharing during verification.

The API caller must supply the expected signer pin from a **previously trusted
signed policy**, not by extracting it from the current download. Production
integration needs a versioned certificate rotation policy; the current Certum
leaf expires in October 2026. Do not hard-code the leaf forever or silently
accept a new signer. This module's two-CA set is not a general TLS root bundle.

## Build and reproduce

`scripts/build-process-trust-probe.ps1` requires explicit `InputsFile`,
`ProjectLock`, `OutputDir`, and `Architecture`. It verifies the foundation,
MSVC 14.44.35207 and Windows SDK 10.0.22621.0 content hashes. Output directories
must be new. Outputs include the console probe, static `.lib`, header and build
evidence. Products consuming the library link `wintrust.lib` and `crypt32.lib`.

`scripts/test-process-trust.py` uses a named disposable Win7 SP1 x86 VM and a
password **file**. It captures actual OS identity and read-only root inventories,
then checks a signed real installer, a corrupted copy, an unsigned binary, a
wrong publisher pin, and a valid file again. It executes only the diagnostic
probe, never the test subjects. This is signature regression, **not** installation
or application business acceptance. The unsigned probe is not a trust anchor.

Keep native environment / root snapshots, subject hashes, probe hash and raw
outputs alongside the report. An x64 build or import audit is not x64 native
execution evidence.

## Required before production integration

1. Finish expiry/timestamp, revoked/offline revocation, alternate-root,
   simultaneous-call and bounded-cancellation cases. Network retrieval may
   block; run verification outside the UI thread, with a supervising deadline.
2. Bind approved signer policy and root-bundle identity to the signed installer
   / update contract. Keep old signed plans intact. Decide rotation before the
   leaf expires; never learn a new pin from an untrusted payload.
3. Rebuild and sign final installer/Agent artifacts, reseal their exact hashes,
   then run native installation, update, rollback and uninstall acceptance.
4. Configure a separately scoped Worker approval public key and approve only
   real evidence. A native probe result alone does not authorize publication.

References: Microsoft Windows SDK `SoftPub.h`, `WinTrust.h`, `WinCrypt.h`,
[WinVerifyTrust](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust),
[exclusive chain engine](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/ns-wincrypt-cert_chain_engine_config).
