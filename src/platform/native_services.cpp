#include "oneui/oneui_c_api.h"
#include "../capi/oneui_c_api_internal.h"
#include "../capi/oneui_c_api_window_internal.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#endif

struct OneUiTray {
#ifdef _WIN32
    HWND hwnd = nullptr;
    UINT id = 0;
    bool visible = false;
    std::wstring tooltip;
#endif
};

namespace {
using oneui::capi::wideOrEmpty;
using oneui::capi::utf8OrEmpty;
using oneui::capi::utf8FromWide;
#ifdef _WIN32
std::atomic<UINT> gTrayNextId{1};

template <std::size_t N>
void copyWideField(wchar_t (&target)[N], const wchar_t* text) {
    if (!text) {
        target[0] = L'\0';
        return;
    }
    std::wcsncpy(target, text, N - 1);
    target[N - 1] = L'\0';
}

constexpr wchar_t kPromptWindowClass[] = L"OneUiPlatformPromptWindow";
constexpr int kPromptEditId = 1001;

struct PromptDialogState {
    HWND dialog = nullptr;
    HWND edit = nullptr;
    int maxChars = 0;
    bool completed = false;
    bool accepted = false;
    std::wstring value;
};

void secureClear(std::wstring& value) {
    if (!value.empty()) {
        SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
    }
    value.clear();
}

void capturePromptValue(PromptDialogState* state) {
    if (!state || !state->edit) {
        return;
    }
    const int length = std::clamp(GetWindowTextLengthW(state->edit), 0, state->maxChars);
    secureClear(state->value);
    state->value.resize(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(state->edit, state->value.data(), length + 1);
    state->value.resize(static_cast<std::size_t>(std::max(0, copied)));
}

void completePrompt(PromptDialogState* state, bool accepted) {
    if (!state || state->completed) {
        return;
    }
    state->accepted = accepted;
    state->completed = true;
    ShowWindow(state->dialog, SW_HIDE);
}

LRESULT CALLBACK promptWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PromptDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = static_cast<PromptDialogState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        if (state) {
            state->dialog = hwnd;
        }
    }

    switch (message) {
    case WM_COMMAND:
        if (LOWORD(wParam) == kPromptEditId && HIWORD(wParam) == EN_CHANGE) {
            capturePromptValue(state);
            return 0;
        }
        if (LOWORD(wParam) == IDOK) {
            completePrompt(state, true);
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            completePrompt(state, false);
            return 0;
        }
        break;
    case WM_CLOSE:
        completePrompt(state, false);
        return 0;
    case WM_DESTROY:
        if (state) {
            state->completed = true;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ensurePromptWindowClass() {
    static std::once_flag once;
    static bool registered = false;
    std::call_once(once, [] {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = promptWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = kPromptWindowClass;
        registered = RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    });
    return registered;
}

const wchar_t* promptAcceptLabel() {
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE ? L"确定" : L"OK";
}

const wchar_t* promptCancelLabel() {
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE ? L"取消" : L"Cancel";
}

int promptDpi(HWND owner) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    const auto getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (owner && getDpiForWindow) {
        return static_cast<int>(getDpiForWindow(owner));
    }
    HDC deviceContext = GetDC(owner);
    const int dpi = deviceContext ? GetDeviceCaps(deviceContext, LOGPIXELSX) : 96;
    if (deviceContext) {
        ReleaseDC(owner, deviceContext);
    }
    return dpi;
}

NOTIFYICONDATAW trayData(const OneUiTray* tray) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = tray ? tray->hwnd : nullptr;
    data.uID = tray ? tray->id : 0;
    return data;
}
#endif

} // namespace

int oneui_window_file_dialog_utf8(
    OneUiWindow* window,
    const OneUiFileDialogOptionsUtf8* options,
    char* buffer,
    std::size_t bufferLength,
    std::size_t* requiredLength) {
#ifdef _WIN32
    if (requiredLength) {
        *requiredLength = 0;
    }
    if (!options || options->filter_count > 64 ||
        (options->filter_count > 0 && !options->filters)) {
        return -1;
    }

    try {
        struct ComApartment final {
            HRESULT result = CoInitializeEx(
                nullptr,
                COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            ~ComApartment() {
                if (SUCCEEDED(result)) {
                    CoUninitialize();
                }
            }
        } apartment;
        if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) {
            return -1;
        }

        IFileDialog* dialog = nullptr;
        HRESULT createResult = E_INVALIDARG;
        if (options->mode == OneUiFileDialogSaveFile) {
            createResult = CoCreateInstance(
                CLSID_FileSaveDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog));
        } else if (options->mode == OneUiFileDialogOpenFile ||
                   options->mode == OneUiFileDialogSelectFolder) {
            createResult = CoCreateInstance(
                CLSID_FileOpenDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog));
        }
        if (FAILED(createResult) || !dialog) {
            return -1;
        }
        struct DialogRelease final {
            IFileDialog* value = nullptr;
            ~DialogRelease() {
                if (value) {
                    value->Release();
                }
            }
        } dialogRelease{dialog};

        // Keep OneUI file-dialog state isolated from other shell clients. When
        // the caller provides an explicit start directory, discard a stale
        // filename/location from an earlier dialog before applying that path.
        // Otherwise Windows can reopen directly into a persistent
        // "Location is unavailable" error even though the requested folder is
        // valid.
        static constexpr GUID oneUiFileDialogClientGuid{
            0x9fd06618,
            0x7a7d,
            0x49c7,
            {0x9b, 0x76, 0xa1, 0xe4, 0xc4, 0xdd, 0xb8, 0x3a},
        };
        HRESULT configureResult = dialog->SetClientGuid(oneUiFileDialogClientGuid);

        DWORD flags = 0;
        if (SUCCEEDED(configureResult)) {
            configureResult = dialog->GetOptions(&flags);
        }
        if (SUCCEEDED(configureResult)) {
            flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
            if (options->mode == OneUiFileDialogOpenFile) {
                flags |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
            } else if (options->mode == OneUiFileDialogSelectFolder) {
                flags |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
            } else if (options->confirm_overwrite) {
                flags |= FOS_OVERWRITEPROMPT;
            }
            configureResult = dialog->SetOptions(flags);
        }

        const std::wstring title = utf8OrEmpty(options->title);
        if (SUCCEEDED(configureResult) && !title.empty()) {
            configureResult = dialog->SetTitle(title.c_str());
        }

        const std::wstring initialDirectory = utf8OrEmpty(options->initial_directory);
        if (SUCCEEDED(configureResult) && !initialDirectory.empty()) {
            // A missing persisted state is not an error for this operation on
            // every supported Windows build. Clearing is intentionally
            // best-effort; the explicit folder below remains authoritative.
            dialog->ClearClientData();
            IShellItem* initialFolder = nullptr;
            const HRESULT folderResult = SHCreateItemFromParsingName(
                initialDirectory.c_str(),
                nullptr,
                IID_PPV_ARGS(&initialFolder));
            if (SUCCEEDED(folderResult) && initialFolder) {
                configureResult = dialog->SetFolder(initialFolder);
                initialFolder->Release();
            } else {
                configureResult = folderResult;
            }
        }

        const std::wstring defaultName = utf8OrEmpty(options->default_name);
        if (SUCCEEDED(configureResult) && !defaultName.empty() &&
            options->mode != OneUiFileDialogSelectFolder) {
            configureResult = dialog->SetFileName(defaultName.c_str());
        }

        const std::wstring defaultExtension = utf8OrEmpty(options->default_extension);
        if (SUCCEEDED(configureResult) && !defaultExtension.empty() &&
            options->mode == OneUiFileDialogSaveFile) {
            const wchar_t* extension = defaultExtension.front() == L'.'
                ? defaultExtension.c_str() + 1
                : defaultExtension.c_str();
            configureResult = dialog->SetDefaultExtension(extension);
        }

        std::vector<std::wstring> filterNames;
        std::vector<std::wstring> filterPatterns;
        std::vector<COMDLG_FILTERSPEC> filterSpecs;
        if (SUCCEEDED(configureResult) && options->filter_count > 0 &&
            options->mode != OneUiFileDialogSelectFolder) {
            filterNames.reserve(options->filter_count);
            filterPatterns.reserve(options->filter_count);
            for (std::size_t index = 0; index < options->filter_count; ++index) {
                filterNames.push_back(utf8OrEmpty(options->filters[index].name));
                filterPatterns.push_back(utf8OrEmpty(options->filters[index].pattern));
            }
            filterSpecs.reserve(options->filter_count);
            for (std::size_t index = 0; index < options->filter_count; ++index) {
                if (filterNames[index].empty() || filterPatterns[index].empty()) {
                    configureResult = E_INVALIDARG;
                    break;
                }
                filterSpecs.push_back({filterNames[index].c_str(), filterPatterns[index].c_str()});
            }
            if (SUCCEEDED(configureResult)) {
                configureResult = dialog->SetFileTypes(
                    static_cast<UINT>(filterSpecs.size()),
                    filterSpecs.data());
                if (SUCCEEDED(configureResult)) {
                    configureResult = dialog->SetFileTypeIndex(1);
                }
            }
        }

        int result = -1;
        if (SUCCEEDED(configureResult)) {
            HWND owner = window && window->window
                ? reinterpret_cast<HWND>(window->window->nativeHandle())
                : nullptr;
            const HRESULT showResult = dialog->Show(owner);
            if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                result = 0;
            } else if (SUCCEEDED(showResult)) {
                IShellItem* selectedItem = nullptr;
                if (SUCCEEDED(dialog->GetResult(&selectedItem)) && selectedItem) {
                    PWSTR selectedPath = nullptr;
                    if (SUCCEEDED(selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath)) &&
                        selectedPath) {
                        const std::string selectedUtf8 = utf8FromWide(selectedPath);
                        const std::size_t required = selectedUtf8.size() + 1;
                        if (requiredLength) {
                            *requiredLength = required;
                        }
                        if (!buffer || bufferLength < required) {
                            result = -2;
                        } else {
                            std::memcpy(buffer, selectedUtf8.data(), selectedUtf8.size());
                            buffer[selectedUtf8.size()] = '\0';
                            result = 1;
                        }
                        CoTaskMemFree(selectedPath);
                    }
                    selectedItem->Release();
                }
            }
        }

        return result;
    } catch (...) {
        return -1;
    }
#else
    (void)window;
    (void)options;
    (void)buffer;
    (void)bufferLength;
    if (requiredLength) {
        *requiredLength = 0;
    }
    return -1;
#endif
}

int oneui_window_pick_folder(OneUiWindow* window, const wchar_t* title, wchar_t* out, int outLen) {
#ifdef _WIN32
    if (!out || outLen <= 0) {
        return 0;
    }
    out[0] = L'\0';
    HWND owner = nullptr;
    if (window && window->window) {
        owner = reinterpret_cast<HWND>(window->window->nativeHandle());
    }

    const bool comInited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
    int picked = 0;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        if (title && title[0] != L'\0') {
            dialog->SetTitle(title);
        }
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    lstrcpynW(out, path, outLen);
                    picked = 1;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (comInited) {
        CoUninitialize();
    }
    return picked;
#else
    (void)window;
    (void)title;
    (void)out;
    (void)outLen;
    return 0;
#endif
}

int oneui_window_confirm(OneUiWindow* window, const wchar_t* title, const wchar_t* message) {
#ifdef _WIN32
    HWND owner = nullptr;
    if (window && window->window) {
        owner = reinterpret_cast<HWND>(window->window->nativeHandle());
    }
    const wchar_t* dialogTitle = title && title[0] != L'\0' ? title : L"Confirm";
    const wchar_t* dialogMessage = message && message[0] != L'\0' ? message : L"Continue?";
    const int result = MessageBoxW(
        owner,
        dialogMessage,
        dialogTitle,
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2 | MB_APPLMODAL);
    return result == IDYES ? 1 : 0;
#else
    (void)window;
    (void)title;
    (void)message;
    return 0;
#endif
}

int oneui_window_prompt_text(
    OneUiWindow* window,
    const wchar_t* title,
    const wchar_t* message,
    const wchar_t* initialValue,
    const wchar_t* placeholder,
    int password,
    wchar_t* out,
    int outLen) {
#ifdef _WIN32
    if (!out || outLen <= 0) {
        return 0;
    }
    SecureZeroMemory(out, static_cast<std::size_t>(outLen) * sizeof(wchar_t));
    if (!ensurePromptWindowClass()) {
        return 0;
    }

    HWND owner = nullptr;
    if (window && window->window) {
        owner = reinterpret_cast<HWND>(window->window->nativeHandle());
    }

    const int dpi = promptDpi(owner);
    const auto scaled = [dpi](int value) { return MulDiv(value, dpi, 96); };
    const int width = scaled(480);
    const int height = scaled(205);

    RECT anchor{};
    if (!owner || !GetWindowRect(owner, &anchor)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0);
    }
    const int left = anchor.left + std::max(0L, (anchor.right - anchor.left - width) / 2);
    const int top = anchor.top + std::max(0L, (anchor.bottom - anchor.top - height) / 2);

    PromptDialogState state;
    state.maxChars = outLen - 1;
    const wchar_t* dialogTitle = title && title[0] != L'\0' ? title : L"Input";
    HWND dialog = CreateWindowExW(
        WS_EX_CONTROLPARENT | WS_EX_DLGMODALFRAME,
        kPromptWindowClass,
        dialogTitle,
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        left,
        top,
        width,
        height,
        owner,
        nullptr,
        GetModuleHandleW(nullptr),
        &state);
    if (!dialog) {
        return 0;
    }

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const int margin = scaled(18);
    HWND messageControl = CreateWindowExW(
        0,
        L"STATIC",
        message ? message : L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin,
        scaled(18),
        width - margin * 2,
        scaled(48),
        dialog,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);

    DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    if (password != 0) {
        editStyle |= ES_PASSWORD;
    }
    state.edit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        initialValue ? initialValue : L"",
        editStyle,
        margin,
        scaled(73),
        width - margin * 2,
        scaled(27),
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPromptEditId)),
        GetModuleHandleW(nullptr),
        nullptr);
    capturePromptValue(&state);
    SendMessageW(state.edit, EM_SETLIMITTEXT, static_cast<WPARAM>(state.maxChars), 0);
    if (placeholder && placeholder[0] != L'\0') {
        SendMessageW(state.edit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(placeholder));
    }

    const int buttonWidth = scaled(86);
    const int buttonHeight = scaled(28);
    const int buttonTop = scaled(119);
    HWND acceptButton = CreateWindowExW(
        0,
        L"BUTTON",
        promptAcceptLabel(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        width - margin - buttonWidth * 2 - scaled(10),
        buttonTop,
        buttonWidth,
        buttonHeight,
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
        GetModuleHandleW(nullptr),
        nullptr);
    HWND cancelButton = CreateWindowExW(
        0,
        L"BUTTON",
        promptCancelLabel(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        width - margin - buttonWidth,
        buttonTop,
        buttonWidth,
        buttonHeight,
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
        GetModuleHandleW(nullptr),
        nullptr);

    for (HWND control : {messageControl, state.edit, acceptButton, cancelButton}) {
        if (control) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
    }

    const bool restoreOwner = owner && IsWindowEnabled(owner);
    if (restoreOwner) {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
    SetForegroundWindow(dialog);
    SetFocus(state.edit);
    SendMessageW(state.edit, EM_SETSEL, 0, -1);

    MSG nativeMessage{};
    while (!state.completed) {
        const BOOL messageResult = GetMessageW(&nativeMessage, nullptr, 0, 0);
        if (messageResult <= 0) {
            if (messageResult == 0) {
                PostQuitMessage(static_cast<int>(nativeMessage.wParam));
            }
            break;
        }
        if (!IsDialogMessageW(dialog, &nativeMessage)) {
            TranslateMessage(&nativeMessage);
            DispatchMessageW(&nativeMessage);
        }
    }

    const bool accepted = state.completed && state.accepted;
    if (accepted) {
        lstrcpynW(out, state.value.c_str(), outLen);
    }
    SetWindowTextW(state.edit, L"");
    DestroyWindow(dialog);
    if (restoreOwner) {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    secureClear(state.value);

    return accepted ? 1 : 0;
#else
    (void)window;
    (void)title;
    (void)message;
    (void)initialValue;
    (void)placeholder;
    (void)password;
    (void)out;
    (void)outLen;
    return 0;
#endif
}

OneUiTray* oneui_tray_create(OneUiWindow* window, const wchar_t* tooltip) {
#ifdef _WIN32
    if (!window || !window->window) {
        return nullptr;
    }
    auto hwnd = reinterpret_cast<HWND>(window->window->nativeHandle());
    if (!hwnd) {
        return nullptr;
    }

    auto tray = std::make_unique<OneUiTray>();
    tray->hwnd = hwnd;
    tray->id = gTrayNextId.fetch_add(1);
    tray->tooltip = wideOrEmpty(tooltip);
    return tray.release();
#else
    (void)window;
    (void)tooltip;
    return nullptr;
#endif
}

void oneui_tray_destroy(OneUiTray* tray) {
    if (!tray) {
        return;
    }
    oneui_tray_hide(tray);
    delete tray;
}

int oneui_tray_show(OneUiTray* tray) {
#ifdef _WIN32
    if (!tray || !tray->hwnd) {
        return 0;
    }
    if (tray->visible) {
        return 1;
    }

    auto data = trayData(tray);
    // NIF_MESSAGE：让托盘点击/右键事件投递到窗口过程（Win32Window 里处理还原/菜单）。
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    data.uCallbackMessage = oneui::kTrayCallbackMessage;
    // 优先用可执行文件里嵌入的品牌图标（资源 ID 1）；取不到再退回系统默认图标。
    HICON appIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    data.hIcon = appIcon ? appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    copyWideField(data.szTip, tray->tooltip.c_str());
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        return 0;
    }

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    tray->visible = true;
    return 1;
#else
    (void)tray;
    return 0;
#endif
}

int oneui_tray_hide(OneUiTray* tray) {
#ifdef _WIN32
    if (!tray || !tray->hwnd || !tray->visible) {
        return 1;
    }

    auto data = trayData(tray);
    if (!Shell_NotifyIconW(NIM_DELETE, &data)) {
        return 0;
    }
    tray->visible = false;
    return 1;
#else
    (void)tray;
    return 0;
#endif
}

void oneui_tray_set_tooltip(OneUiTray* tray, const wchar_t* tooltip) {
#ifdef _WIN32
    if (!tray || !tray->hwnd) {
        return;
    }
    tray->tooltip = wideOrEmpty(tooltip);
    if (!tray->visible) {
        return;
    }

    auto data = trayData(tray);
    data.uFlags = NIF_TIP;
    copyWideField(data.szTip, tray->tooltip.c_str());
    Shell_NotifyIconW(NIM_MODIFY, &data);
#else
    (void)tray;
    (void)tooltip;
#endif
}

int oneui_tray_show_notification(OneUiTray* tray, const wchar_t* title, const wchar_t* message) {
#ifdef _WIN32
    if (!tray || !tray->hwnd) {
        return 0;
    }
    if (!tray->visible && !oneui_tray_show(tray)) {
        return 0;
    }

    auto data = trayData(tray);
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    copyWideField(data.szInfoTitle, title);
    copyWideField(data.szInfo, message);
    return Shell_NotifyIconW(NIM_MODIFY, &data) ? 1 : 0;
#else
    (void)tray;
    (void)title;
    (void)message;
    return 0;
#endif
}
