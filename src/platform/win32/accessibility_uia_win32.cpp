#include "accessibility_uia_win32.h"
#include "accessibility_win32.h"
#include "compat_win32.h"

#include <UIAutomation.h>
#include <wrl/client.h>

#include <array>
#include <climits>
#include <cmath>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oneui::win32_accessibility {
namespace {

using Microsoft::WRL::ComPtr;

struct AutomationApi {
    AutomationApi() : module(win32::loadSystemLibrary(L"UIAutomationCore.dll")) {
        if (!module) return;
        reply = reinterpret_cast<decltype(reply)>(GetProcAddress(module, "UiaReturnRawElementProvider"));
        disconnect = reinterpret_cast<decltype(disconnect)>(GetProcAddress(module, "UiaDisconnectProvider"));
        host = reinterpret_cast<decltype(host)>(GetProcAddress(module, "UiaHostProviderFromHwnd"));
        legacy = reinterpret_cast<decltype(legacy)>(GetProcAddress(module, "UiaProviderFromIAccessible"));
    }
    ~AutomationApi() { if (module) FreeLibrary(module); }
    HMODULE module;
    decltype(&UiaReturnRawElementProvider) reply = nullptr;
    decltype(&UiaDisconnectProvider) disconnect = nullptr;
    decltype(&UiaHostProviderFromHwnd) host = nullptr;
    decltype(&UiaProviderFromIAccessible) legacy = nullptr;
};

struct FragmentTree {
    FragmentTree(Provider* shared, HWND window) : accessible(shared), hwnd(window) {}

    using FragmentReference = ComPtr<IRawElementProviderFragment>;

    std::vector<FragmentReference> pruneDetachedFragments() {
        const auto revision = accessible->fragmentRevision();
        if (revision == childRevision) return {};
        childRevision = revision;
        std::vector<FragmentReference> detached;
        for (auto entry = children.begin(); entry != children.end();) {
            if (accessible->includesFragment(entry->first)) ++entry;
            else {
                // Release any system pattern adapters after the map is stable.
                detached.push_back(std::move(entry->second));
                entry = children.erase(entry);
            }
        }
        return detached;
    }

    AutomationApi api;
    ComPtr<Provider> accessible;
    HWND hwnd;
    IRawElementProviderFragment* root = nullptr;
    IRawElementProviderFragmentRoot* fragmentRoot = nullptr;
    // A stable COM identity allows UIA to reuse the same provider and its
    // pattern adapters across enumerations of the current projection.
    std::unordered_map<LONG, FragmentReference> children;
    std::uint64_t childRevision = 0;
};

CONTROLTYPEID controlType(LONG role) {
    switch (role) {
    case ROLE_SYSTEM_PUSHBUTTON: return UIA_ButtonControlTypeId;
    case ROLE_SYSTEM_STATICTEXT: return UIA_TextControlTypeId;
    case ROLE_SYSTEM_TEXT: return UIA_EditControlTypeId;
    case ROLE_SYSTEM_CHECKBUTTON: return UIA_CheckBoxControlTypeId;
    case ROLE_SYSTEM_RADIOBUTTON: return UIA_RadioButtonControlTypeId;
    case ROLE_SYSTEM_GROUPING: return UIA_GroupControlTypeId;
    case ROLE_SYSTEM_COMBOBOX: return UIA_ComboBoxControlTypeId;
    case ROLE_SYSTEM_SLIDER: return UIA_SliderControlTypeId;
    case ROLE_SYSTEM_PROGRESSBAR: return UIA_ProgressBarControlTypeId;
    case ROLE_SYSTEM_PAGETAB: return UIA_TabItemControlTypeId;
    case ROLE_SYSTEM_PAGETABLIST: return UIA_TabControlTypeId;
    case ROLE_SYSTEM_LIST: return UIA_ListControlTypeId;
    case ROLE_SYSTEM_LISTITEM: return UIA_ListItemControlTypeId;
    case ROLE_SYSTEM_TABLE: return UIA_TableControlTypeId;
    case ROLE_SYSTEM_ROW:
    case ROLE_SYSTEM_CELL: return UIA_DataItemControlTypeId;
    case ROLE_SYSTEM_WINDOW:
    case ROLE_SYSTEM_DIALOG: return UIA_WindowControlTypeId;
    default: return UIA_CustomControlTypeId;
    }
}

class Fragment final : public IRawElementProviderSimple, public IRawElementProviderFragment,
                       public IRawElementProviderFragmentRoot, public IInvokeProvider,
                       public IValueProvider {
public:
    Fragment(std::shared_ptr<FragmentTree> tree, LONG id) : tree_(std::move(tree)), id_(id) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IRawElementProviderSimple))
            *out = static_cast<IRawElementProviderSimple*>(this);
        else if (id == __uuidof(IRawElementProviderFragment))
            *out = static_cast<IRawElementProviderFragment*>(this);
        else if (id == __uuidof(IRawElementProviderFragmentRoot) && isRoot())
            *out = static_cast<IRawElementProviderFragmentRoot*>(this);
        else if (id == __uuidof(IInvokeProvider) && !isRoot())
            *out = static_cast<IInvokeProvider*>(this);
        else if (id == __uuidof(IValueProvider) && !isRoot())
            *out = static_cast<IValueProvider*>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto refs = --refs_;
        if (!refs) delete this;
        return refs;
    }

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* out) override {
        if (!out) return E_POINTER;
        // Widget state remains owned by the original window's COM apartment.
        *out = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID pattern, IUnknown** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        const LONG role = readRole();
        switch (pattern) {
        case UIA_InvokePatternId:
            if (role == ROLE_SYSTEM_PUSHBUTTON)
                return QueryInterface(__uuidof(IInvokeProvider), reinterpret_cast<void**>(out));
            break;
        case UIA_ValuePatternId:
            if (role == ROLE_SYSTEM_TEXT)
                return QueryInterface(__uuidof(IValueProvider), reinterpret_cast<void**>(out));
            break;
        }
        // Ask the system adapter for compatibility patterns only. Ordinary UIA
        // discovery, properties, hit testing and traversal stay on the native
        // fragment and do not send a request through the HWND's MSAA proxy.
        if (!legacy_ && tree_->api.legacy) {
            tree_->api.legacy(tree_->accessible.Get(), id_, UIA_PFIA_DEFAULT, legacy_.GetAddressOf());
        }
        return legacy_ ? legacy_->GetPatternProvider(pattern, out) : S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID property, VARIANT* out) override {
        if (!out) return E_POINTER;
        VariantInit(out);
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        switch (property) {
        case UIA_NamePropertyId:
            return readString(&Provider::get_accName, out);
        case UIA_HelpTextPropertyId:
        case UIA_FullDescriptionPropertyId:
            return readString(&Provider::get_accDescription, out);
        case UIA_ControlTypePropertyId:
            out->vt = VT_I4;
            out->lVal = controlType(readRole());
            return S_OK;
        case UIA_IsControlElementPropertyId:
        case UIA_IsContentElementPropertyId:
            return boolean(out, true);
        case UIA_HasKeyboardFocusPropertyId:
            return boolean(out, (readState() & STATE_SYSTEM_FOCUSED) != 0);
        case UIA_IsKeyboardFocusablePropertyId:
            return boolean(out, (readState() & STATE_SYSTEM_FOCUSABLE) != 0);
        case UIA_IsEnabledPropertyId:
            return boolean(out, (readState() & STATE_SYSTEM_UNAVAILABLE) == 0);
        case UIA_IsOffscreenPropertyId:
            return boolean(out, (readState() & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN)) != 0);
        case UIA_IsPasswordPropertyId:
            return boolean(out, (readState() & STATE_SYSTEM_PROTECTED) != 0);
        default:
            return S_OK;
        }
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        return isRoot() ? tree_->api.host(tree_->hwnd, out) : S_OK;
    }

    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction, IRawElementProviderFragment** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        if (isRoot()) {
            if (direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild)
                return returnFragment(tree_->accessible->firstFragment(direction == NavigateDirection_LastChild), out);
        } else {
            if (direction == NavigateDirection_Parent) return returnRoot(out);
            if (direction == NavigateDirection_NextSibling || direction == NavigateDirection_PreviousSibling)
                return returnFragment(tree_->accessible->siblingFragment(id_, direction == NavigateDirection_NextSibling ? 1 : -1), out);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        if (isRoot()) return S_OK; // The HWND host supplies its root identity.
        std::array<LONG, 2> runtimeId{UiaAppendRuntimeId, id_};
        auto* result = SafeArrayCreateVector(VT_I4, 0, static_cast<ULONG>(runtimeId.size()));
        if (!result) return E_OUTOFMEMORY;
        for (LONG index = 0; index < static_cast<LONG>(runtimeId.size()); ++index) {
            const auto status = SafeArrayPutElement(result, &index, &runtimeId[index]);
            if (FAILED(status)) {
                SafeArrayDestroy(result);
                return status;
            }
        }
        *out = result;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* out) override {
        if (!out) return E_POINTER;
        *out = {};
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        LONG x = 0, y = 0, width = 0, height = 0;
        const auto status = tree_->accessible->accLocation(&x, &y, &width, &height, child());
        *out = {double(x), double(y), double(width), double(height)};
        return status;
    }

    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        return available() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE SetFocus() override {
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        if (isRoot()) {
            ::SetFocus(tree_->hwnd);
            return ::GetFocus() == tree_->hwnd ? S_OK : UIA_E_INVALIDOPERATION;
        }
        if (readState() & STATE_SYSTEM_UNAVAILABLE) return UIA_E_ELEMENTNOTENABLED;
        return tree_->accessible->accSelect(SELFLAG_TAKEFOCUS, child()) == S_OK ? S_OK : UIA_E_INVALIDOPERATION;
    }

    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        *out = tree_->fragmentRoot;
        (*out)->AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        constexpr auto limit = static_cast<double>(LONG_MAX);
        if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > limit || std::abs(y) > limit) return S_OK;
        VARIANT hit{};
        tree_->accessible->accHitTest(std::lround(x), std::lround(y), &hit);
        if (const LONG fragment = tree_->accessible->fragmentFromChild(hit)) return returnFragment(fragment, out);
        UiaRect bounds{};
        get_BoundingRectangle(&bounds);
        if (x >= bounds.left && y >= bounds.top && x < bounds.left + bounds.width && y < bounds.top + bounds.height)
            return returnRoot(out);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        VARIANT focus{};
        tree_->accessible->get_accFocus(&focus);
        if (focus.vt == VT_I4 && focus.lVal == CHILDID_SELF) return returnRoot(out);
        return returnFragment(tree_->accessible->fragmentFromChild(focus), out);
    }

    HRESULT STDMETHODCALLTYPE Invoke() override {
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        if (readState() & STATE_SYSTEM_UNAVAILABLE) return UIA_E_ELEMENTNOTENABLED;
        return tree_->accessible->accDoDefaultAction(child()) == S_OK ? S_OK : UIA_E_INVALIDOPERATION;
    }

    HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) override {
        if (!value) return E_INVALIDARG;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        if (readState() & STATE_SYSTEM_UNAVAILABLE) return UIA_E_ELEMENTNOTENABLED;
        BSTR text = SysAllocString(value);
        if (!text) return E_OUTOFMEMORY;
        const HRESULT status = tree_->accessible->put_accValue(child(), text);
        SysFreeString(text);
        return status == S_FALSE ? UIA_E_INVALIDOPERATION : status;
    }

    HRESULT STDMETHODCALLTYPE get_Value(BSTR* out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        return tree_->accessible->get_accValue(child(), out);
    }

    HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* out) override {
        if (!out) return E_POINTER;
        *out = TRUE;
        if (!available()) return UIA_E_ELEMENTNOTAVAILABLE;
        *out = (readState() & STATE_SYSTEM_READONLY) != 0;
        return S_OK;
    }

    FragmentTree& tree() const { return *tree_; }

private:
    bool isRoot() const { return id_ == CHILDID_SELF; }
    bool available() const { return tree_->root && tree_->accessible->fragmentAvailable(id_); }

    VARIANT child() const {
        VARIANT value{};
        value.vt = VT_I4;
        value.lVal = id_;
        return value;
    }

    LONG readRole() const {
        VARIANT role{};
        tree_->accessible->get_accRole(child(), &role);
        return role.vt == VT_I4 ? role.lVal : ROLE_SYSTEM_CLIENT;
    }

    LONG readState() const {
        VARIANT state{};
        tree_->accessible->get_accState(child(), &state);
        return state.vt == VT_I4 ? state.lVal : 0;
    }

    using StringProperty = HRESULT (STDMETHODCALLTYPE Provider::*)(VARIANT, BSTR*);
    HRESULT readString(StringProperty property, VARIANT* out) {
        BSTR value = nullptr;
        const HRESULT result = (tree_->accessible.Get()->*property)(child(), &value);
        if (value) {
            out->vt = VT_BSTR;
            out->bstrVal = value;
        }
        return result == S_FALSE ? S_OK : result;
    }

    static HRESULT boolean(VARIANT* out, bool value) {
        out->vt = VT_BOOL;
        out->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
        return S_OK;
    }

    HRESULT returnRoot(IRawElementProviderFragment** out) const {
        *out = tree_->root;
        (*out)->AddRef();
        return S_OK;
    }

    HRESULT returnFragment(LONG id, IRawElementProviderFragment** out) {
        // Release detached providers only after handing out the new reference.
        // A release may reenter window teardown through a system adapter.
        const auto detached = tree_->pruneDetachedFragments();
        if (!id) return S_OK;
        const auto existing = tree_->children.find(id);
        if (existing != tree_->children.end()) return existing->second.CopyTo(out);
        ComPtr<IRawElementProviderFragment> result;
        result.Attach(new (std::nothrow) Fragment(tree_, id));
        if (!result) return E_OUTOFMEMORY;
        const auto added = tree_->children.emplace(id, std::move(result));
        return added.first->second.CopyTo(out);
    }

    std::shared_ptr<FragmentTree> tree_;
    LONG id_;
    std::atomic<ULONG> refs_{1};
    ComPtr<IRawElementProviderSimple> legacy_;
};

} // namespace

AutomationProvider::~AutomationProvider() { disconnect(); }

LRESULT AutomationProvider::reply(HWND window, WPARAM wp, LPARAM lp, IAccessible* accessible) {
    if (unsupported_) return 0;
    if (!root_) {
        auto tree = std::make_shared<FragmentTree>(static_cast<Provider*>(accessible), window);
        if (!tree->api.reply || !tree->api.disconnect || !tree->api.host) {
            unsupported_ = true;
            return 0;
        }
        auto* root = new Fragment(tree, CHILDID_SELF);
        tree->root = root;
        tree->fragmentRoot = root;
        root_ = root;
    }
    const ComPtr<IRawElementProviderSimple> root(root_);
    const auto& api = static_cast<Fragment*>(root.Get())->tree().api;
    return api.reply(window, wp, lp, root.Get());
}

void AutomationProvider::disconnect() {
    if (auto* root = std::exchange(root_, nullptr)) {
        auto& tree = static_cast<Fragment*>(root)->tree();
        // A stale child may still have external references. It retains the
        // shared MSAA provider but cannot navigate back to a dead HWND.
        tree.root = nullptr;
        tree.fragmentRoot = nullptr;
        tree.children.clear();
        tree.api.disconnect(root);
        root->Release();
    }
}

} // namespace oneui::win32_accessibility
