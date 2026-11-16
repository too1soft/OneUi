#include "oneui/platform/window.h"
#include "oneui/canvas.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
constexpr std::uint32_t untouched = 0x0055aa77;

void check(bool value, const char* name) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

class Pattern final : public oneui::Widget {
public:
    oneui::Color left{25, 59, 95};
    oneui::Color right{214, 99, 44};
    oneui::Color bottom{40, 95, 56};
    void paint(oneui::Canvas& canvas) override {
        const auto bounds = frame();
        canvas.fillRect({0, 0, bounds.width, bounds.height}, left);
        canvas.fillRect({bounds.width / 2, 0, bounds.width / 2, bounds.height}, right);
        canvas.fillRect({0, bounds.height * 2 / 3, bounds.width, bounds.height / 3}, bottom);
    }
};

class Bitmap {
public:
    Bitmap(int width, int height) : width(width), height(height) {
        dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
            reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!dc || !bitmap) throw std::runtime_error("Cannot create capture DC");
        previous = SelectObject(dc, bitmap);
        clear();
    }
    ~Bitmap() {
        SelectObject(dc, previous);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;
    void clear() { std::fill(pixels, pixels + width * height, untouched); }
    std::uint32_t at(int x, int y) const { return pixels[y * width + x] & 0xffffff; }
    HDC dc = nullptr;
    int width, height;
private:
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    std::uint32_t* pixels = nullptr;
};

std::uint32_t rgb(oneui::Color color) {
    return (std::uint32_t(color.r) << 16) | (std::uint32_t(color.g) << 8) | color.b;
}

void verifyPattern(const Bitmap& image, const Pattern& pattern, int width, int height,
                   int x = 0, int y = 0) {
    check(image.at(x + width / 5, y + height / 5) == rgb(pattern.left), "left client pixels");
    check(image.at(x + width * 4 / 5, y + height / 5) == rgb(pattern.right), "right client pixels");
    check(image.at(x + width / 2, y + height * 4 / 5) == rgb(pattern.bottom), "bottom client pixels and orientation");
}

RECT clientRect(HWND hwnd) {
    RECT rect{};
    if (!GetClientRect(hwnd, &rect) || rect.right < 20 || rect.bottom < 20)
        throw std::runtime_error("Invalid native client geometry");
    return rect;
}

void verify() {
    auto pattern = std::make_shared<Pattern>();
    oneui::WindowOptions options;
    options.title = L"OneUI production WM_PRINTCLIENT contract";
    options.width = 320;
    options.height = 240;
    options.borderless = true;
    auto window = oneui::Window::create(options);
    window->initialize();
    window->setContent(pattern);
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    auto rect = clientRect(hwnd);
    {
        Bitmap image(rect.right, rect.bottom);
        SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT);
        verifyPattern(image, *pattern, rect.right, rect.bottom);
        std::cout << "never-presented hidden window checked\n";
        image.clear();
        SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT | PRF_CHECKVISIBLE);
        check(image.at(rect.right / 5, rect.bottom / 5) == untouched, "hidden PRF_CHECKVISIBLE does not draw");
    }
    window->show();
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    std::cout << "renderer=" << static_cast<int>(window->rendererInfo().backend) << '\n';
    {
        Bitmap image(rect.right, rect.bottom);
        check(PrintWindow(hwnd, image.dc, PW_CLIENTONLY) != FALSE, "PrintWindow client call succeeds");
        const bool printWindowPixels = image.at(rect.right / 5, rect.bottom / 5) == rgb(pattern->left);
        std::cout << "PrintWindow-client-pixels=" << printWindowPixels << '\n';
        if (window->rendererInfo().backend == oneui::RenderBackend::Software)
            verifyPattern(image, *pattern, rect.right, rect.bottom);
        SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT);
        verifyPattern(image, *pattern, rect.right, rect.bottom);
        pattern->left = {119, 37, 163};
        // No native paint or event pump between the model edit and the request.
        SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT);
        verifyPattern(image, *pattern, rect.right, rect.bottom);
        check(SendMessageW(hwnd, WM_PRINTCLIENT, 0, PRF_CLIENT) == 0, "null DC is harmless");
        std::cout << "visible client and unpresented model edit checked\n";
    }
    {
        RECT outer{};
        GetWindowRect(hwnd, &outer);
        POINT origin{};
        ClientToScreen(hwnd, &origin);
        Bitmap image(outer.right - outer.left, outer.bottom - outer.top);
        SendMessageW(hwnd, WM_PRINT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
        verifyPattern(image, *pattern, rect.right, rect.bottom, origin.x - outer.left, origin.y - outer.top);
    }
    {
        Bitmap image(rect.right + 20, rect.bottom + 20);
        SetViewportOrgEx(image.dc, 7, 9, nullptr);
        const int saved = SaveDC(image.dc);
        IntersectClipRect(image.dc, 0, 0, rect.right / 2, rect.bottom);
        SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT);
        POINT origin{};
        GetViewportOrgEx(image.dc, &origin);
        check(origin.x == 7 && origin.y == 9, "caller viewport origin preserved");
        check(image.at(7 + rect.right / 5, 9 + rect.bottom / 5) == rgb(pattern->left), "caller origin applied");
        check(image.at(7 + rect.right * 4 / 5, 9 + rect.bottom / 5) == untouched, "caller clip preserved");
        RestoreDC(image.dc, saved);
    }
    {
        oneui::WindowOptions coverOptions = options;
        coverOptions.title = L"Independent occluding OneUI window";
        auto cover = oneui::Window::create(coverOptions);
        cover->initialize();
        auto other = std::make_shared<Pattern>();
        other->left = {3, 5, 7};
        other->right = {9, 11, 13};
        other->bottom = {15, 17, 19};
        cover->setContent(other);
        cover->show();
        RECT outer{};
        GetWindowRect(hwnd, &outer);
        SetWindowPos(static_cast<HWND>(cover->nativeHandle()), HWND_TOP, outer.left, outer.top,
            outer.right - outer.left, outer.bottom - outer.top, SWP_NOACTIVATE);
        Bitmap image(rect.right, rect.bottom);
        SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT);
        verifyPattern(image, *pattern, rect.right, rect.bottom);
        cover->close();
        std::cout << "occluding window identity checked\n";
    }
    check(window->setClientSize({403, 267}), "native resize succeeds");
    window->setContentScale(2);
    rect = clientRect(hwnd);
    {
        Bitmap image(rect.right, rect.bottom);
        SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT);
        verifyPattern(image, *pattern, rect.right, rect.bottom);
        const DWORD handles = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        for (int i = 0; i < 64; ++i)
            SendMessageW(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(image.dc), PRF_CLIENT);
        check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= handles + 1, "repeated capture does not leak GDI objects");
    }
    window->close();
    std::cout << "checks=" << checks << " failures=" << failures << '\n';
}

int childWindow(bool software) {
    SetEnvironmentVariableW(L"ONEUI_RENDERER", software ? L"software" : L"auto");
    oneui::WindowOptions options;
    options.title = L"OneUI print child " + std::to_wstring(GetCurrentProcessId());
    options.width = 320;
    options.height = 240;
    auto window = oneui::Window::create(options);
    window->initialize();
    window->setContent(std::make_shared<Pattern>());
    window->show();
    return window->run();
}

void verifyCrossProcess() {
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr, executable, 32768))
        throw std::runtime_error("Cannot resolve owned test executable");
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --print-child-software";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup, &process))
        throw std::runtime_error("Cannot start owned print child");
    struct ProcessGuard {
        PROCESS_INFORMATION value;
        ~ProcessGuard() {
            if (WaitForSingleObject(value.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(value.hProcess, 2);
                WaitForSingleObject(value.hProcess, 5000);
            }
            CloseHandle(value.hThread);
            CloseHandle(value.hProcess);
        }
    } guard{process};
    HWND hwnd = nullptr;
    const auto title = L"OneUI print child " + std::to_wstring(process.dwProcessId);
    const ULONGLONG deadline = GetTickCount64() + 5000;
    while (!hwnd && GetTickCount64() < deadline &&
           WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
        hwnd = FindWindowW(nullptr, title.c_str());
        if (hwnd && !IsWindowVisible(hwnd)) hwnd = nullptr;
        if (!hwnd) Sleep(10);
    }
    if (!hwnd) throw std::runtime_error("Owned print child did not expose its window");
    const auto rect = clientRect(hwnd);
    Bitmap image(rect.right, rect.bottom);
    // PrintWindow, unlike passing a private memory DC through SendMessage,
    // provides the system's cross-process capture path.
    check(PrintWindow(hwnd, image.dc, PW_CLIENTONLY) != FALSE,
        "cross-process software PrintWindow completes");
    std::cout << "cross-process-sample=" << std::hex << image.at(rect.right / 5, rect.bottom / 5)
              << std::dec << '\n';
    Pattern expected;
    verifyPattern(image, expected, rect.right, rect.bottom);
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    check(WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0, "owned print child closes normally");
    DWORD code = 2;
    GetExitCodeProcess(process.hProcess, &code);
    check(code == 0, "owned print child exits zero");
    std::cout << "cross-process production software client pixels checked\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--print-child") return childWindow(false);
        if (argc > 1 && std::string(argv[1]) == "--print-child-software") return childWindow(true);
        verify();
        verifyCrossProcess();
        std::cout << "total-checks=" << checks << " failures=" << failures << '\n';
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
