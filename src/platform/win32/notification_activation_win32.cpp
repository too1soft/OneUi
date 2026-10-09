#include "notification_activation_win32.h"
#include "internal/runtime_error.h"
#include "internal/unicode.h"
#include "notification_callback_abi.h"
#include <cstring>
#include <shobjidl.h>
#include <wrl/implements.h>

namespace oneui::win32 {
namespace {
class Activator final : public Microsoft::WRL::RuntimeClass<
                            Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                            INotificationActivationCallback> {
    std::string id_;
    platform::NotificationBackend::Activation callback_;

  public:
    Activator(std::string id, platform::NotificationBackend::Activation callback)
        : id_(std::move(id)), callback_(std::move(callback)) {}
    HRESULT STDMETHODCALLTYPE Activate(LPCWSTR appId, LPCWSTR arguments,
                                       const NOTIFICATION_USER_INPUT_DATA *, ULONG) override {
        try {
            if (!appId || !arguments || wcsnlen(appId, 129) > 128 ||
                wcsnlen(arguments, 9001) > 9000 || unicode::toUtf8(appId) != id_)
                return E_INVALIDARG;
            ApplicationActivation value;
            if (!platform::decodeActivation(unicode::toUtf8(arguments), value))
                return E_INVALIDARG;
            if (callback_)
                callback_(std::move(value));
            return S_OK;
        } catch (...) {
            detail::recordCurrentException("Notification.COM.activate");
            return E_FAIL;
        }
    }
};
class Factory final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IClassFactory> {
    std::string id_;
    platform::NotificationBackend::Activation callback_;

  public:
    Factory(std::string id, platform::NotificationBackend::Activation callback)
        : id_(std::move(id)), callback_(std::move(callback)) {}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID iid, void **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (outer)
            return CLASS_E_NOAGGREGATION;
        try {
            auto object = Microsoft::WRL::Make<Activator>(id_, callback_);
            return object ? object.CopyTo(iid, out) : E_OUTOFMEMORY;
        } catch (...) {
            return E_OUTOFMEMORY;
        }
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) override { return S_OK; }
};
constexpr ULONG_PTR activationMessage = 0x4f4e4555;
} // namespace
NotificationActivation::NotificationActivation(const ApplicationOptions &options,
                                               platform::NotificationBackend::Activation callback)
    : id_(options.id), callback_(std::move(callback)) {
    const auto clsid = notificationActivatorId(id_);
    windowClass_ = L"OneUI.Notification." + notificationGuidString(clsid);
    mutex_ = CreateMutexW(nullptr, TRUE, (L"Local\\" + windowClass_).c_str());
    const bool existed = mutex_ && GetLastError() == ERROR_ALREADY_EXISTS;
    ownsMutex_ = mutex_ && !existed;
    if (existed) {
        auto acquired = WaitForSingleObject(mutex_, 0);
        ownsMutex_ = acquired == WAIT_OBJECT_0 || acquired == WAIT_ABANDONED;
    }
    if (mutex_ && !ownsMutex_) {
        primary_ = false;
        HWND existing = nullptr;
        for (int attempt = 0; attempt < 50 && !existing; ++attempt) {
            existing = FindWindowExW(HWND_MESSAGE, nullptr, windowClass_.c_str(), nullptr);
            if (!existing)
                Sleep(20);
        }
        if (existing) {
            const auto payload = platform::encodeActivation({});
            COPYDATASTRUCT data{activationMessage, static_cast<DWORD>(payload.size() + 1),
                                const_cast<char *>(payload.c_str())};
            DWORD_PTR reply = 0;
            if (SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
                                    SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &reply) &&
                reply) {
                return;
            }
        }
        return;
    }
    WNDCLASSW type{};
    type.lpfnWndProc = dispatch;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = windowClass_.c_str();
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return;
    window_ = CreateWindowExW(0, windowClass_.c_str(), L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                              type.hInstance, this);
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    comInitialized_ = SUCCEEDED(initialized);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        return;
    SetCurrentProcessExplicitAppUserModelID(unicode::fromUtf8(id_).c_str());
    auto factory = Microsoft::WRL::Make<Factory>(id_, callback_);
    if (factory)
        CoRegisterClassObject(clsid, factory.Get(), CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE,
                              &cookie_);
}
NotificationActivation::~NotificationActivation() {
    if (cookie_)
        CoRevokeClassObject(cookie_);
    if (window_)
        DestroyWindow(window_);
    if (primary_)
        UnregisterClassW(windowClass_.c_str(), GetModuleHandleW(nullptr));
    if (mutex_) {
        if (ownsMutex_)
            ReleaseMutex(mutex_);
        CloseHandle(mutex_);
    }
    if (comInitialized_)
        CoUninitialize();
}
LRESULT CALLBACK NotificationActivation::dispatch(HWND window, UINT message, WPARAM wparam,
                                                  LPARAM lparam) {
    if (message == WM_NCCREATE)
        SetWindowLongPtrW(
            window, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams));
    auto *self =
        reinterpret_cast<NotificationActivation *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_COPYDATA && self) {
        auto *data = reinterpret_cast<COPYDATASTRUCT *>(lparam);
        if (!data || data->dwData != activationMessage || !data->lpData || data->cbData < 1 ||
            data->cbData > 9001)
            return FALSE;
        auto *bytes = static_cast<const char *>(data->lpData);
        if (bytes[data->cbData - 1] != 0 || std::memchr(bytes, 0, data->cbData - 1))
            return FALSE;
        try {
            ApplicationActivation value;
            if (!platform::decodeActivation({bytes, data->cbData - 1}, value))
                return FALSE;
            if (self->callback_)
                self->callback_(std::move(value));
            return TRUE;
        } catch (...) {
            detail::recordCurrentException("Notification.instance.activate");
            return FALSE;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace oneui::win32
