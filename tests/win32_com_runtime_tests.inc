// Driver-owned message-loop threads may receive UIA hooks without calling COM.
// They need an implicit MTA, while the application thread keeps its own STA.
#include <thread>

bool graphicsCallbacksHaveAnImplicitMta() {
    const auto ole = GetModuleHandleW(L"ole32.dll");
    if (!ole || !GetProcAddress(ole, "CoIncrementMTAUsage")) {
        // Windows 7 has no process MTA usage API; its backend remains supported,
        // but this Windows 8+ runtime contract is checked only when available.
        return true;
    }
    auto probe = [] {
        bool implicit = false;
        std::thread worker([&] {
            APTTYPE apartment;
            APTTYPEQUALIFIER qualifier;
            implicit = SUCCEEDED(CoGetApartmentType(&apartment, &qualifier))
                && apartment == APTTYPE_MTA && qualifier == APTTYPEQUALIFIER_IMPLICIT_MTA;
        });
        worker.join();
        return implicit;
    };
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    window->initialize();
    APTTYPE apartment;
    APTTYPEQUALIFIER qualifier;
    const bool ownerSta = SUCCEEDED(CoGetApartmentType(&apartment, &qualifier))
        && (apartment == APTTYPE_STA || apartment == APTTYPE_MAINSTA);
    const bool during = probe();
    window->close();
    window.reset();
    // GPU helper threads can outlive the last window and still pump messages.
    return ownerSta && during && probe();
}
