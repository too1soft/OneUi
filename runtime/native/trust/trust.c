/* Win7 SP1 ABI, including the original WINTRUST_DATA / chain engine layouts. */
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define NTDDI_VERSION 0x06010100
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <string.h>
#include "trust.h"
#include "trust-roots.generated.h"

typedef struct policy_context {
    const uint8_t *expected_signer;
    BOOL callback_completed;
} policy_context;

static HRESULT last_error(void) {
    DWORD error = GetLastError();
    return error ? HRESULT_FROM_WIN32(error) : E_FAIL;
}

static HRESULT signer_policy(PWTD_GENERIC_CHAIN_POLICY_SIGNER_INFO signer,
    BOOL timestamp, BOOL commercial) {
    CERT_CHAIN_POLICY_PARA parameters = {0};
    CERT_CHAIN_POLICY_STATUS status = {0};
    AUTHENTICODE_EXTRA_CERT_CHAIN_POLICY_PARA auth = {0};
    AUTHENTICODE_TS_EXTRA_CERT_CHAIN_POLICY_PARA ts = {0};
    if (!signer || signer->dwError || !signer->pChainContext)
        return signer && signer->dwError ? (HRESULT)signer->dwError : TRUST_E_SUBJECT_NOT_TRUSTED;
    parameters.cbSize = sizeof(parameters);
    status.cbSize = sizeof(status);
    /* No registry-controlled ignore-revocation or ignore-expiration flags. */
    if (timestamp) {
        ts.cbSize = sizeof(ts);
        ts.fCommercial = commercial;
        parameters.pvExtraPolicyPara = &ts;
    } else {
        auth.cbSize = sizeof(auth);
        auth.pSignerInfo = signer->pMsgSignerInfo;
        parameters.pvExtraPolicyPara = &auth;
    }
    if (!CertVerifyCertificateChainPolicy(timestamp ? CERT_CHAIN_POLICY_AUTHENTICODE_TS : CERT_CHAIN_POLICY_AUTHENTICODE,
            signer->pChainContext, &parameters, &status)) return last_error();
    return (HRESULT)status.dwError;
}

static HRESULT WINAPI verify_policy(PCRYPT_PROVIDER_DATA provider, DWORD step_error,
    DWORD registry_flags, DWORD count, PWTD_GENERIC_CHAIN_POLICY_SIGNER_INFO *signers,
    void *argument) {
    policy_context *context = (policy_context *)argument;
    PWTD_GENERIC_CHAIN_POLICY_SIGNER_INFO signer;
    PCCERT_CONTEXT certificate;
    BYTE digest[32];
    DWORD digest_size = sizeof(digest), index;
    HRESULT result;
    (void)registry_flags;
    if (step_error) return (HRESULT)step_error;
    /* Our current release format has exactly one SHA-256 signer and a verified
     * timestamp. Do not silently accept an unexpected / ambiguous signer. */
    if (!context || !provider || count != 1 || !signers || !signers[0]) return TRUST_E_SUBJECT_NOT_TRUSTED;
    signer = signers[0];
    if (!signer->pMsgSignerInfo || !signer->pMsgSignerInfo->HashAlgorithm.pszObjId ||
        strcmp(signer->pMsgSignerInfo->HashAlgorithm.pszObjId, szOID_NIST_sha256) != 0)
        return NTE_BAD_ALGID;
    result = signer_policy(signer, FALSE, FALSE);
    if (result != S_OK) return result;
    if (!signer->pChainContext->cChain || !signer->pChainContext->rgpChain[0] ||
        !signer->pChainContext->rgpChain[0]->cElement) return CERT_E_CHAINING;
    certificate = signer->pChainContext->rgpChain[0]->rgpElement[0]->pCertContext;
    if (!CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, NULL,
        certificate->pbCertEncoded, certificate->cbCertEncoded, digest, &digest_size)) return last_error();
    if (digest_size != 32 || memcmp(digest, context->expected_signer, 32) != 0) return TRUST_E_SUBJECT_NOT_TRUSTED;
    if (!signer->cCounterSigner || !signer->rgpCounterSigner) return TRUST_E_TIME_STAMP;
    for (index = 0; index < signer->cCounterSigner; ++index) {
        result = signer_policy(signer->rgpCounterSigner[index], TRUE, FALSE);
        if (result != S_OK) return result;
    }
    context->callback_completed = TRUE;
    return S_OK;
}

uint32_t wc_verify_publisher(const wchar_t *path, const uint8_t expected_signer_sha256[32]) {
    HCERTSTORE roots = NULL;
    HCERTCHAINENGINE engine = NULL;
    HANDLE file = INVALID_HANDLE_VALUE;
    CERT_CHAIN_ENGINE_CONFIG engine_config = {0};
    CERT_CHAIN_PARA signer_parameters = {0}, timestamp_parameters = {0};
    WTD_GENERIC_CHAIN_POLICY_CREATE_INFO signer_chain = {0}, timestamp_chain = {0};
    WTD_GENERIC_CHAIN_POLICY_DATA policy = {0};
    WINTRUST_FILE_INFO file_info = {0};
    WINTRUST_DATA data = {0};
    GUID action = WINTRUST_ACTION_GENERIC_CHAIN_VERIFY;
    policy_context context = {0};
    LPSTR code_usage = szOID_PKIX_KP_CODE_SIGNING, time_usage = szOID_PKIX_KP_TIMESTAMP_SIGNING;
    LONG result = E_INVALIDARG;
    if (!path || !*path || !expected_signer_sha256) return (uint32_t)result;
    /* Deny writes/deletes during digest + certificate verification. */
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return (uint32_t)last_error();
    roots = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
    if (!roots) { result = last_error(); goto cleanup; }
    if (!CertAddEncodedCertificateToStore(roots, X509_ASN_ENCODING, certum_root,
            sizeof(certum_root), CERT_STORE_ADD_NEW, NULL) ||
        !CertAddEncodedCertificateToStore(roots, X509_ASN_ENCODING, digicert_root,
            sizeof(digicert_root), CERT_STORE_ADD_NEW, NULL)) {
        result = last_error(); goto cleanup;
    }
    engine_config.cbSize = sizeof(engine_config);
    engine_config.hExclusiveRoot = roots;
    engine_config.dwUrlRetrievalTimeout = 10000;
    if (!CertCreateCertificateChainEngine(&engine_config, &engine)) { result = last_error(); goto cleanup; }
    signer_parameters.cbSize = sizeof(signer_parameters);
    signer_parameters.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    signer_parameters.RequestedUsage.Usage.cUsageIdentifier = 1;
    signer_parameters.RequestedUsage.Usage.rgpszUsageIdentifier = &code_usage;
    timestamp_parameters = signer_parameters;
    timestamp_parameters.RequestedUsage.Usage.rgpszUsageIdentifier = &time_usage;
    signer_chain.cbSize = sizeof(signer_chain);
    signer_chain.hChainEngine = engine;
    signer_chain.pChainPara = &signer_parameters;
    signer_chain.dwFlags = CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | CERT_CHAIN_DISABLE_AUTH_ROOT_AUTO_UPDATE;
    timestamp_chain = signer_chain;
    timestamp_chain.pChainPara = &timestamp_parameters;
    context.expected_signer = expected_signer_sha256;
    policy.cbSize = sizeof(policy);
    policy.pSignerChainInfo = &signer_chain;
    policy.pCounterSignerChainInfo = &timestamp_chain;
    policy.pfnPolicyCallback = verify_policy;
    policy.pvPolicyArg = &context;
    file_info.cbStruct = sizeof(file_info);
    file_info.pcwszFilePath = path;
    file_info.hFile = file;
    data.cbStruct = sizeof(data);
    data.pPolicyCallbackData = &policy;
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file_info;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | WTD_DISABLE_MD2_MD4;
    result = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
    if (result == 0 && !context.callback_completed) result = TRUST_E_SUBJECT_NOT_TRUSTED;
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
cleanup:
    if (engine) CertFreeCertificateChainEngine(engine);
    if (roots) CertCloseStore(roots, 0);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return (uint32_t)result;
}
