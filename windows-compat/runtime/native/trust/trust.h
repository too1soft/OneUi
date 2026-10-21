#ifndef WINDOWS_COMPAT_PROCESS_TRUST_H
#define WINDOWS_COMPAT_PROCESS_TRUST_H
#include <stdint.h>
#include <wchar.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Experimental native verifier. Not enabled by any product until acceptance.
 * Only zero is success. The caller must obtain expected_signer_sha256 from a
 * trusted, signed policy, never from the file currently being checked.
 * Verification is process-local and never installs certificates. */
uint32_t wc_verify_publisher(const wchar_t *path,
    const uint8_t expected_signer_sha256[32]);

#ifdef __cplusplus
}
#endif
#endif
