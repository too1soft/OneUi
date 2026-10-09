#include "drop_files_win32.h"
#include "platform/shared/native_dock_drag.h"
#include <cstring>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>
#include <wrl.h>

namespace oneui::platform {
namespace {
using namespace Microsoft::WRL;
CLIPFORMAT dockFormat() {
    static const auto format = static_cast<CLIPFORMAT>(RegisterClipboardFormatA(dockMime));
    return format;
}
FORMATETC format() { return {dockFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL}; }
class DragData final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IDataObject> {
  public:
    std::string payload;
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *value) override {
        return value && value->cfFormat == dockFormat() && value->dwAspect == DVASPECT_CONTENT &&
                       (value->tymed & TYMED_HGLOBAL)
                   ? S_OK
                   : DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC *value, STGMEDIUM *result) override {
        if (!result)
            return E_POINTER;
        *result = {};
        if (FAILED(QueryGetData(value)))
            return DV_E_FORMATETC;
        auto memory = GlobalAlloc(GMEM_MOVEABLE, payload.size() + 1);
        if (!memory)
            return E_OUTOFMEMORY;
        auto bytes = GlobalLock(memory);
        if (!bytes) {
            GlobalFree(memory);
            return E_OUTOFMEMORY;
        }
        std::memcpy(bytes, payload.c_str(), payload.size() + 1);
        GlobalUnlock(memory);
        result->tymed = TYMED_HGLOBAL;
        result->hGlobal = memory;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *, STGMEDIUM *) override {
        return DATA_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *, FORMATETC *out) override {
        if (out)
            out->ptd = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC *, STGMEDIUM *, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC **out) override {
        if (direction != DATADIR_GET)
            return E_NOTIMPL;
        auto value = format();
        return SHCreateStdEnumFmtEtc(1, &value, out);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *, DWORD, IAdviseSink *, DWORD *) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }
};
class DragSource final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IDropSource> {
  public:
    HWND window = nullptr;
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape || !IsWindow(window) || (keys & MK_RBUTTON))
            return DRAGDROP_S_CANCEL;
        return keys & MK_LBUTTON ? S_OK : DRAGDROP_S_DROP;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};
std::string payload(IDataObject *data) {
    if (!data)
        return {};
    auto value = format();
    STGMEDIUM medium{};
    if (FAILED(data->GetData(&value, &medium)))
        return {};
    struct Release {
        STGMEDIUM &value;
        ~Release() { ReleaseStgMedium(&value); }
    } release{medium};
    if (medium.tymed != TYMED_HGLOBAL || !medium.hGlobal)
        return {};
    const auto length = GlobalSize(medium.hGlobal);
    if (!length || length > 4096)
        return {};
    auto bytes = static_cast<const char *>(GlobalLock(medium.hGlobal));
    if (!bytes)
        return {};
    struct Unlock {
        HGLOBAL value;
        ~Unlock() { GlobalUnlock(value); }
    } unlock{medium.hGlobal};
    std::string result;
    const auto end = static_cast<const char *>(std::memchr(bytes, 0, length));
    if (end)
        result.assign(bytes, end);
    return result;
}
struct HostState {
    HWND native = nullptr;
    Window *window = nullptr;
    DockDragCallbacks callbacks;
    std::string incoming;
    bool fileDrop = false;
    Point point(POINTL screen) const {
        POINT point{screen.x, screen.y};
        ScreenToClient(native, &point);
        const float scale = window ? window->dpiScale() : 1;
        return {point.x / scale, point.y / scale};
    }
    bool offer(POINTL screen, bool commit) {
        if (!window || window->closed() || !IsWindowEnabled(native) || incoming.empty() ||
            !callbacks.drop)
            return false;
        try {
            return callbacks.drop(incoming, point(screen), commit);
        } catch (...) {
            return false;
        }
    }
};
class DropTarget final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IDropTarget> {
  public:
    std::weak_ptr<HostState> owner;
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *data, DWORD, POINTL point,
                                        DWORD *effect) override try {
        if (!effect)
            return E_POINTER;
        auto host = owner.lock();
        if (!host) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }
        host->incoming = payload(data);
        // Preserve WM_DROPFILES consumers when OLE owns the same top-level HWND.
        FORMATETC files{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        host->fileDrop = host->incoming.empty() && data && SUCCEEDED(data->QueryGetData(&files)) &&
                         (GetWindowLongPtrW(host->native, GWL_EXSTYLE) & WS_EX_ACCEPTFILES);
        *effect = host->fileDrop              ? (*effect & DROPEFFECT_COPY)
                  : host->offer(point, false) ? (*effect & DROPEFFECT_MOVE)
                                              : DROPEFFECT_NONE;
        return S_OK;
    } catch (...) {
        if (effect)
            *effect = DROPEFFECT_NONE;
        return E_UNEXPECTED;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL point, DWORD *effect) override {
        if (!effect)
            return E_POINTER;
        auto host = owner.lock();
        *effect = host && host->fileDrop              ? (*effect & DROPEFFECT_COPY)
                  : host && host->offer(point, false) ? (*effect & DROPEFFECT_MOVE)
                                                      : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        if (auto host = owner.lock()) {
            host->incoming.clear();
            host->fileDrop = false;
            if (host->callbacks.leave) {
                try {
                    host->callbacks.leave();
                } catch (...) {
                }
            }
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject *data, DWORD, POINTL point, DWORD *effect) override
        try {
        if (!effect)
            return E_POINTER;
        auto host = owner.lock();
        *effect = DROPEFFECT_NONE;
        if (!host)
            return S_OK;
        if (host->fileDrop && IsWindow(host->native)) {
            FORMATETC files{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            STGMEDIUM medium{};
            if (SUCCEEDED(data->GetData(&files, &medium))) {
                if (medium.tymed == TYMED_HGLOBAL &&
                    GlobalSize(medium.hGlobal) <= 16u * 1024u * 1024u) {
                    auto copy = static_cast<HGLOBAL>(OleDuplicateData(medium.hGlobal, CF_HDROP, 0));
                    if (copy) {
                        auto drop = GlobalSize(copy) >= sizeof(DROPFILES)
                                        ? static_cast<DROPFILES *>(GlobalLock(copy))
                                        : nullptr;
                        if (drop && validDropFiles(drop, GlobalSize(copy))) {
                            POINT client{point.x, point.y};
                            ScreenToClient(host->native, &client);
                            drop->pt = client;
                            drop->fNC = FALSE;
                            GlobalUnlock(copy);
                            SendMessageW(host->native, WM_DROPFILES, reinterpret_cast<WPARAM>(copy),
                                         0);
                            *effect = DROPEFFECT_COPY;
                        } else {
                            if (drop)
                                GlobalUnlock(copy);
                            GlobalFree(copy);
                        }
                    }
                }
                ReleaseStgMedium(&medium);
            }
        } else {
            host->incoming = payload(data);
            if (host->offer(point, true))
                *effect = DROPEFFECT_MOVE;
        }
        DragLeave();
        return S_OK;
    } catch (...) {
        if (effect)
            *effect = DROPEFFECT_NONE;
        return E_UNEXPECTED;
    }
};
class WinDockDrag final : public NativeDockDrag, public std::enable_shared_from_this<WinDockDrag> {
    std::shared_ptr<HostState> state_;
    ComPtr<DropTarget> target_;
    Window::EventSubscription closed_;
    bool ole_ = false, registered_ = false, dragging_ = false;

  public:
    WinDockDrag(Window &window, DockDragCallbacks callbacks)
        : state_(std::make_shared<HostState>()) {
        state_->window = &window;
        state_->native = static_cast<HWND>(window.nativeHandle());
        state_->callbacks = std::move(callbacks);
        ole_ = SUCCEEDED(OleInitialize(nullptr));
        if (!ole_)
            return;
        target_ = Make<DropTarget>();
        target_->owner = state_;
        registered_ = SUCCEEDED(RegisterDragDrop(state_->native, target_.Get()));
        closed_ = window.observeClosed([weak = std::weak_ptr<HostState>(state_)] {
            if (auto state = weak.lock()) {
                state->window = nullptr;
                state->callbacks = {};
            }
        });
    }
    ~WinDockDrag() override {
        closed_.reset();
        state_->callbacks = {};
        state_->window = nullptr;
        if (registered_)
            RevokeDragDrop(state_->native);
        target_.Reset();
        if (ole_)
            OleUninitialize();
    }
    bool available() const { return registered_; }
    bool start(std::string bytes, std::function<void(NativeDragResult, Point)> done) override {
        if (!registered_ || dragging_ || !state_->window || bytes.empty() || bytes.size() > 1024 ||
            !(GetKeyState(VK_LBUTTON) & 0x8000))
            return false;
        auto hold = shared_from_this();
        auto data = Make<DragData>();
        auto source = Make<DragSource>();
        data->payload = std::move(bytes);
        source->window = state_->native;
        ReleaseCapture();
        dragging_ = true;
        DWORD effect = 0;
        auto result = DoDragDrop(data.Get(), source.Get(), DROPEFFECT_MOVE, &effect);
        dragging_ = false;
        POINT position{};
        GetCursorPos(&position);
        auto status = result == DRAGDROP_S_CANCEL ? NativeDragResult::Cancelled
                      : result == DRAGDROP_S_DROP
                          ? (effect == DROPEFFECT_MOVE ? NativeDragResult::Dropped
                                                       : NativeDragResult::Unhandled)
                          : NativeDragResult::Failed;
        if (done)
            done(status, {float(position.x), float(position.y)});
        return true;
    }
};
} // namespace
std::shared_ptr<NativeDockDrag> makeWinDockDrag(Window &window, DockDragCallbacks callbacks) {
    window.initialize();
    auto host = std::make_shared<WinDockDrag>(window, std::move(callbacks));
    return host->available() ? host : nullptr;
}
} // namespace oneui::platform
