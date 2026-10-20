#include <stdio.h>
#include "windows_compat.h"
int main(void) {
    wc_identity os;
    if (!wc_detect(&os)) { puts("Unsupported or unknown system; installation blocked."); return 2; }
    printf("Windows %lu.%lu.%lu SP%u native=%u process=%u default-profile=%u\n",
        os.major, os.minor, os.build, os.service_pack, os.native_arch, os.process_arch, os.default_profile);
    return 0;
}
