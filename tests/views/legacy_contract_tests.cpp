#include "fixture.h"
using namespace viewtest;
#include "legacy_contracts.inc"
#include "legacy_runtime.inc"
#include "legacy_reload.inc"

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        legacytest::all(argv[1]);
        std::cout << "PASS exact baseline/branch IR contracts, records and retained event reload\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
