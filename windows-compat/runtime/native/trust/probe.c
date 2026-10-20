#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "trust.h"

static int compare_hashes(const void *left, const void *right) {
    return strcmp((const char *)left, (const char *)right);
}

static int print_roots(DWORD location) {
    HCERTSTORE store;
    PCCERT_CONTEXT cert = NULL;
    char (*hashes)[65];
    DWORD count = 0, index, byte, size;
    BYTE digest[32];
    int success = 0;
    store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
        location | CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG, L"ROOT");
    if (!store) return 0;
    hashes = (char (*)[65])HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 8192 * 65);
    if (!hashes) { CertCloseStore(store, 0); return 0; }
    while ((cert = CertEnumCertificatesInStore(store, cert)) != NULL) {
        if (count == 8192) goto cleanup;
        size = sizeof(digest);
        if (!CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, NULL,
            cert->pbCertEncoded, cert->cbCertEncoded, digest, &size) || size != 32) goto cleanup;
        for (byte = 0; byte < 32; ++byte) {
            static const char hex[] = "0123456789abcdef";
            hashes[count][byte*2] = hex[digest[byte] >> 4];
            hashes[count][byte*2+1] = hex[digest[byte] & 15];
        }
        ++count;
    }
    if (GetLastError() != CRYPT_E_NOT_FOUND) goto cleanup;
    qsort(hashes, count, 65, compare_hashes);
    putchar('[');
    for (index = 0; index < count; ++index) printf("%s\"%s\"", index ? "," : "", hashes[index]);
    putchar(']');
    success = 1;
cleanup:
    if (cert) CertFreeCertificateContext(cert);
    HeapFree(GetProcessHeap(), 0, hashes);
    CertCloseStore(store, 0);
    return success;
}

static int environment(void) {
    typedef LONG (WINAPI *version_fn)(OSVERSIONINFOEXW *);
    union { FARPROC symbol; version_fn call; } version;
    OSVERSIONINFOEXW os = {0};
    SYSTEM_INFO system = {0};
    version.symbol = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    os.dwOSVersionInfoSize = sizeof(os);
    if (!version.symbol || version.call(&os) != 0) return 1;
    GetNativeSystemInfo(&system);
    printf("{\"scope\":\"native-environment\",\"stage\":\"complete\",\"major\":%lu,\"minor\":%lu,\"build\":%lu,\"servicePack\":%u,\"nativeArchitecture\":%u,\"processBits\":%zu,\"machineRoots\":",
        os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber, os.wServicePackMajor,
        system.wProcessorArchitecture, sizeof(void*)*8);
    if (!print_roots(CERT_SYSTEM_STORE_LOCAL_MACHINE)) return 1;
    printf(",\"userRoots\":");
    if (!print_roots(CERT_SYSTEM_STORE_CURRENT_USER)) return 1;
    puts("}");
    return 0;
}

/* Diagnostic only: command-line pin is not an installation authorization.
 * Products must use an already trusted signed policy or a compiled-in pin. */
int wmain(int argc, wchar_t **argv) {
    uint8_t signer[32];
    uint32_t code;
    unsigned int i;
    ULONGLONG started;
    if (argc == 2 && wcscmp(argv[1], L"--environment") == 0) return environment();
    if (argc != 3 || wcslen(argv[2]) != 64) return 2;
    for (i = 0; i < 32; ++i) {
        unsigned int j, byte = 0;
        for (j = 0; j < 2; ++j) {
            wchar_t ch = argv[2][2*i+j];
            unsigned int nibble;
            if (ch >= L'0' && ch <= L'9') nibble = ch-L'0';
            else if (ch >= L'a' && ch <= L'f') nibble = ch-L'a'+10;
            else if (ch >= L'A' && ch <= L'F') nibble = ch-L'A'+10;
            else return 2;
            byte = byte*16 + nibble;
        }
        signer[i] = (uint8_t)byte;
    }
    started = GetTickCount64();
    puts("{\"scope\":\"process-local-authenticode\",\"stage\":\"verifying\"}");
    fflush(stdout);
    code = wc_verify_publisher(argv[1], signer);
    printf("{\"scope\":\"process-local-authenticode\",\"stage\":\"complete\",\"passed\":%s,\"code\":\"0x%08lx\",\"elapsedMs\":%llu,\"processBits\":%zu}\n",
        code == 0 ? "true" : "false", (unsigned long)code, GetTickCount64()-started, sizeof(void*)*8);
    return code == 0 ? 0 : 1;
}
