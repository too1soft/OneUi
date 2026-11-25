#include "angle_d3d11_win32.h"
#include "platform/shared/skia_gl_setup.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <utility>

namespace oneui::win32 {
namespace {

// Private EGL 1.5 ABI. Load ANGLE at runtime so it is an optional Win32 backend.
using Egl = void*;
using EglBool = unsigned int;
using EglEnum = unsigned int;
using EglInt = std::int32_t;
using EglAttrib = std::intptr_t;

constexpr wchar_t kDirectory[] = L"ONEUI_WIN32_ANGLE_DIR";
constexpr wchar_t kComposition[] = L"ONEUI_WIN32_ANGLE_COMPOSITION";
constexpr EglInt kNone = 0x3038, kSurfaceType = 0x3033, kWindowBit = 0x0004;
constexpr EglInt kRenderableType = 0x3040, kGles3Bit = 0x0040, kContextClientVersion = 0x3098;
constexpr EglInt kRedSize = 0x3024, kGreenSize = 0x3023, kBlueSize = 0x3022;
constexpr EglInt kAlphaSize = 0x3021, kStencilSize = 0x3026, kSamples = 0x3031;
constexpr EglInt kDraw = 0x3059, kRead = 0x305a;
constexpr EglInt kWidth = 0x3057, kHeight = 0x3056;
constexpr EglInt kExtensions = 0x3055, kPostSubBufferSupported = 0x30be;
constexpr EglInt kDirectComposition = 0x33a5;
constexpr EglEnum kOpenGlesApi = 0x30a0, kPlatformAngle = 0x3202;
constexpr EglAttrib kAngleType = 0x3203, kDirect3D11 = 0x3208;
constexpr EglAttrib kAngleDeviceType = 0x3209, kHardware = 0x320a;

struct Modules {
    HMODULE gles = nullptr;
    HMODULE egl = nullptr;
    ~Modules() {
        if (egl) FreeLibrary(egl);
        if (gles) FreeLibrary(gles);
    }
};

struct AngleApi {
    Modules modules;
    Egl(WINAPI* platformDisplay)(EglEnum, void*, const EglAttrib*) = nullptr;
    EglBool(WINAPI* initialize)(Egl, EglInt*, EglInt*) = nullptr;
    EglBool(WINAPI* terminate)(Egl) = nullptr;
    EglBool(WINAPI* bindApi)(EglEnum) = nullptr;
    EglEnum(WINAPI* queryApi)() = nullptr;
    EglBool(WINAPI* chooseConfig)(Egl, const EglInt*, Egl*, EglInt, EglInt*) = nullptr;
    EglBool(WINAPI* getConfigAttrib)(Egl, Egl, EglInt, EglInt*) = nullptr;
    Egl(WINAPI* createContext)(Egl, Egl, Egl, const EglInt*) = nullptr;
    EglBool(WINAPI* destroyContext)(Egl, Egl) = nullptr;
    Egl(WINAPI* createWindowSurface)(Egl, Egl, HWND, const EglInt*) = nullptr;
    EglBool(WINAPI* destroySurface)(Egl, Egl) = nullptr;
    EglBool(WINAPI* querySurface)(Egl, Egl, EglInt, EglInt*) = nullptr;
    const char*(WINAPI* queryString)(Egl, EglInt) = nullptr;
    EglBool(WINAPI* makeCurrent)(Egl, Egl, Egl, Egl) = nullptr;
    Egl(WINAPI* currentContext)() = nullptr;
    Egl(WINAPI* currentDisplay)() = nullptr;
    Egl(WINAPI* currentSurface)(EglInt) = nullptr;
    EglBool(WINAPI* swapInterval)(Egl, EglInt) = nullptr;
    EglBool(WINAPI* swap)(Egl, Egl) = nullptr;
    GrGLFuncPtr(WINAPI* getProc)(const char*) = nullptr;

    template <typename Function>
    bool resolve(Function& target, const char* name) {
        target = reinterpret_cast<Function>(GetProcAddress(modules.egl, name));
        return target != nullptr;
    }

    bool load(const std::filesystem::path& directory) {
        constexpr DWORD search = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
        // Resolve both libraries from the selected directory. libEGL may open
        // its GLES companion by basename, so preload the matching module.
        if (GetModuleHandleW(L"libEGL.dll") || GetModuleHandleW(L"libGLESv2.dll")) return false;
        modules.gles = LoadLibraryExW((directory / L"libGLESv2.dll").c_str(), nullptr, search);
        if (!modules.gles) return false;
        modules.egl = LoadLibraryExW((directory / L"libEGL.dll").c_str(), nullptr, search);
        return modules.egl && resolve(platformDisplay, "eglGetPlatformDisplay") &&
            resolve(initialize, "eglInitialize") && resolve(terminate, "eglTerminate") &&
            resolve(bindApi, "eglBindAPI") && resolve(queryApi, "eglQueryAPI") &&
            resolve(chooseConfig, "eglChooseConfig") && resolve(getConfigAttrib, "eglGetConfigAttrib") &&
            resolve(createContext, "eglCreateContext") && resolve(destroyContext, "eglDestroyContext") &&
            resolve(createWindowSurface, "eglCreateWindowSurface") && resolve(destroySurface, "eglDestroySurface") &&
            resolve(querySurface, "eglQuerySurface") &&
            resolve(queryString, "eglQueryString") &&
            resolve(makeCurrent, "eglMakeCurrent") && resolve(currentContext, "eglGetCurrentContext") &&
            resolve(currentDisplay, "eglGetCurrentDisplay") && resolve(currentSurface, "eglGetCurrentSurface") &&
            resolve(swapInterval, "eglSwapInterval") && resolve(swap, "eglSwapBuffers") &&
            resolve(getProc, "eglGetProcAddress");
    }

    Egl acquireDisplay() {
        std::lock_guard lock(displayMutex);
        if (!users) {
            const EglAttrib attrs[]{kAngleType, kDirect3D11, kAngleDeviceType, kHardware, kNone};
            const auto display = platformDisplay(kPlatformAngle, nullptr, attrs);
            if (!display || !initialize(display, nullptr, nullptr)) return nullptr;
            activeDisplay = display;
        }
        ++users;
        return activeDisplay;
    }

    void releaseDisplay() {
        std::lock_guard lock(displayMutex);
        if (--users == 0) {
            terminate(activeDisplay);
            activeDisplay = nullptr;
        }
    }

private:
    std::mutex displayMutex;
    unsigned users = 0;
    Egl activeDisplay = nullptr;
};

AngleApi* angleApi() {
    static auto* instance = []() -> AngleApi* {
        const DWORD length = GetEnvironmentVariableW(kDirectory, nullptr, 0);
        if (!length) return nullptr;
        std::wstring value(length, L'\0');
        const DWORD written = GetEnvironmentVariableW(kDirectory, value.data(), length);
        if (!written || written >= length) return nullptr;
        value.resize(written);
        const std::filesystem::path directory(value);
        if (!directory.is_absolute()) return nullptr;
        auto api = std::make_unique<AngleApi>();
        // The module stays mapped for the process. Each window still releases
        // its contexts and surfaces, and the last window terminates the display.
        return api->load(directory) ? api.release() : nullptr;
    }();
    return instance;
}

struct PreviousBinding {
    AngleApi& api;
    Egl display = api.currentDisplay(), context = api.currentContext();
    Egl draw = api.currentSurface(kDraw), read = api.currentSurface(kRead);
    EglEnum kind = api.queryApi();

    void restore(Egl activeDisplay) const {
        if (display) {
            api.bindApi(kind);
            api.makeCurrent(display, draw, read, context);
        } else {
            if (api.currentContext()) api.makeCurrent(activeDisplay, nullptr, nullptr, nullptr);
            api.bindApi(kind);
        }
    }
};

bool hasExtension(const char* extensions, std::string_view name) {
    if (!extensions) return false;
    const std::string_view list(extensions);
    auto index = list.find(name);
    while (index != std::string_view::npos) {
        const auto end = index + name.size();
        if ((index == 0 || list[index - 1] == ' ') && (end == list.size() || list[end] == ' ')) return true;
        index = list.find(name, end);
    }
    return false;
}

} // namespace

struct AngleD3D11Context::State {
    using PostSubBuffer = EglBool(WINAPI*)(Egl, Egl, EglInt, EglInt, EglInt, EglInt);
    AngleApi& api;
    Egl display = nullptr, context = nullptr, surface = nullptr;
    EglInt stencil = 0;
    PostSubBuffer postSubBuffer = nullptr;
    bool composed = false;
    bool synced = false;
    std::string device;

    ~State() {
        if (!display) return;
        if (context && api.currentContext() == context) api.makeCurrent(display, nullptr, nullptr, nullptr);
        if (surface) api.destroySurface(display, surface);
        if (context) api.destroyContext(display, context);
        api.releaseDisplay();
    }

    bool current() const {
        return api.currentContext() == context && api.currentSurface(kDraw) == surface &&
               api.currentSurface(kRead) == surface;
    }

    bool bind() const {
        return current() || (api.bindApi(kOpenGlesApi) && api.makeCurrent(display, surface, surface, context));
    }
};

bool AngleD3D11Context::requested() noexcept {
    return GetEnvironmentVariableW(kDirectory, nullptr, 0) > 1;
}

AngleD3D11Context::AngleD3D11Context(std::unique_ptr<State> state) : state_(std::move(state)) {}
AngleD3D11Context::~AngleD3D11Context() = default;

std::unique_ptr<AngleD3D11Context> AngleD3D11Context::open(HWND window, sk_sp<GrDirectContext>& skia) {
    auto* api = angleApi();
    if (!api) return {};
    auto state = std::unique_ptr<State>(new State{*api});
    state->display = api->acquireDisplay();
    if (!state->display) return {};
    Egl config = nullptr;
    EglInt count = 0;
    const EglInt attrs[]{kSurfaceType, kWindowBit, kRenderableType, kGles3Bit, kRedSize, 8, kGreenSize, 8,
                         kBlueSize, 8, kAlphaSize, 8, kStencilSize, 8, kSamples, 0, kNone};
    if (!api->chooseConfig(state->display, attrs, &config, 1, &count) || count != 1 ||
        !api->getConfigAttrib(state->display, config, kStencilSize, &state->stencil)) return {};

    const PreviousBinding previous{*api};
    const EglInt contextAttrs[]{kContextClientVersion, 3, kNone};
    if (!api->bindApi(kOpenGlesApi)) return {};
    state->context = api->createContext(state->display, config, nullptr, contextAttrs);
    const auto* extensions = api->queryString(state->display, kExtensions);
    if (hasExtension(extensions, "EGL_NV_post_sub_buffer")) {
        state->postSubBuffer = reinterpret_cast<State::PostSubBuffer>(api->getProc("eglPostSubBufferNV"));
    }
    wchar_t composition[2]{};
    if (state->context && GetEnvironmentVariableW(kComposition, composition, 2) == 1 && composition[0] == L'1' &&
        hasExtension(extensions, "EGL_ANGLE_direct_composition")) {
        const EglInt composedAttrs[]{kDirectComposition, 1, state->postSubBuffer ? kPostSubBufferSupported : kNone, 1, kNone};
        state->surface = api->createWindowSurface(state->display, config, window, composedAttrs);
        EglInt actual = 0;
        if (state->surface && api->querySurface(state->display, state->surface, kDirectComposition, &actual) && actual == 1) {
            state->composed = true;
        } else if (state->surface) {
            api->destroySurface(state->display, state->surface);
            state->surface = nullptr;
        }
    }
    const EglInt surfaceAttrs[]{kPostSubBufferSupported, 1, kNone};
    if (state->context && !state->surface) state->surface = api->createWindowSurface(state->display, config, window,
        state->postSubBuffer ? surfaceAttrs : nullptr);
    if (!state->surface || !state->bind()) {
        previous.restore(state->display);
        return {};
    }
    EglInt canPost = 0;
    if (state->postSubBuffer && (!api->querySurface(state->display, state->surface, kPostSubBufferSupported, &canPost) || !canPost)) {
        state->postSubBuffer = nullptr;
    }
    auto gl = GrGLMakeAssembledGLESInterface(api, [](void* data, const char* name) {
        auto* loaded = static_cast<AngleApi*>(data);
        if (const auto function = GetProcAddress(loaded->modules.gles, name))
            return reinterpret_cast<GrGLFuncPtr>(function);
        return loaded->getProc(name);
    });
    if (gl) {
        const auto* renderer = gl->fFunctions.fGetString(0x1f01); // GL_RENDERER
        if (renderer) state->device = reinterpret_cast<const char*>(renderer);
    }
    if (!gl || state->device.find("Direct3D11") == std::string::npos) {
        previous.restore(state->display);
        return {};
    }
    if (state->composed) state->device += " via DirectComposition";
    GrContextOptions options;
    options.fPersistentCache = rendering::angleD3D11ShaderCache();
    skia = GrDirectContexts::MakeGL(std::move(gl), options);
    state->synced = api->swapInterval(state->display, 1) != 0;
    previous.restore(state->display);
    return skia ? std::unique_ptr<AngleD3D11Context>(new AngleD3D11Context(std::move(state))) : nullptr;
}

bool AngleD3D11Context::makeCurrent() { return state_->bind(); }
bool AngleD3D11Context::refreshBufferSize(int& width, int& height, bool clientSizeChanged) {
    const int clientWidth = width, clientHeight = height;
    const auto querySize = [&] {
        return state_->api.querySurface(state_->display, state_->surface, kWidth, &width) &&
               state_->api.querySurface(state_->display, state_->surface, kHeight, &height) && width > 0 && height > 0;
    };
    if (!querySize()) return false;
    if (!clientSizeChanged || !state_->postSubBuffer || (width == clientWidth && height == clientHeight)) return true;
    // ANGLE's EGL frontend skips a zero-area update. Its D3D11 backend clips
    // this rectangle at the old buffer edge, then checks the native size
    // without presenting the old buffer. Query again before drawing.
    if (!state_->postSubBuffer(state_->display, state_->surface, width, 0, 1, 1)) {
        state_->postSubBuffer = nullptr;
        return true;
    }
    return querySize();
}
bool AngleD3D11Context::present() { return state_->api.swap(state_->display, state_->surface) != 0; }
bool AngleD3D11Context::vsync() const noexcept { return state_->synced; }
int AngleD3D11Context::stencilBits() const noexcept { return state_->stencil; }
const std::string& AngleD3D11Context::device() const noexcept { return state_->device; }

void AngleD3D11Context::releaseSkia(sk_sp<SkSurface>& window, sk_sp<SkSurface>& retained,
                                    sk_sp<GrDirectContext>& skia) {
    const PreviousBinding previous{state_->api};
    if (skia && !makeCurrent()) skia->abandonContext();
    window.reset();
    retained.reset();
    skia.reset();
    if (previous.context != state_->context) previous.restore(state_->display);
}

} // namespace oneui::win32
