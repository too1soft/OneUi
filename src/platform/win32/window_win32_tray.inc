    void setCloseToTray(bool closeToTray) override {
        closeToTray_ = closeToTray;
        if (!closeToTray_ && !trayEnabled_) {
            hideInternalTrayIcon();
            return;
        }
        ensureCreated();
        showInternalTrayIcon();
    }

    bool trayIconVisible() const override {
        return internalTrayVisible_;
    }

    bool setTrayEnabled(bool enabled) override {
        trayEnabled_ = enabled;
        if (!enabled) { hideInternalTrayIcon(); return true; }
        ensureCreated();
        return showInternalTrayIcon();
    }

    void setOnCloseRequested(std::function<void(bool)> callback) override {
        onCloseRequested_ = std::move(callback);
    }

    bool setTrayMenu(const std::vector<TrayMenuItem>& items) override {
        if (GetCurrentThreadId()!=menuUiThread_ || !callbackWindowAlive_->load() || !validateNativeMenu(items)) return false;
        trayMenu_ = items;
        return true;
    }
    bool setMenuBar(const std::vector<NativeMenuItem>& items) override {
        if (GetCurrentThreadId()!=menuUiThread_ || !callbackWindowAlive_->load() || !validateNativeMenu(items)) return false;
        if (!menuHost_) {
            if (items.empty()) return true;
            menuHost_=std::make_shared<internal::ApplicationMenuHost>();
            auto alive=callbackWindowAlive_;
            menuHost_->setAction([this,alive](int id){if(alive->load()){auto callback=onMenuAction_;if(callback)callback(id);}});
            setContent(applicationContent_);
        }
        menuHost_->setItems(items);requestRedraw();return true;
    }
    void setOnMenuAction(std::function<void(int)> action) override { onMenuAction_=std::move(action); }
    bool setTrayIcon(const TrayIcon& icon) override {
        if (GetCurrentThreadId()!=menuUiThread_ || !callbackWindowAlive_->load() || !validateTrayIcon(icon)) return false;
        HICON replacement=nullptr;
        if (!icon.rgba.empty()) {
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=icon.width;info.bmiHeader.biHeight=-int(icon.height);
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            void* pixels=nullptr;
            HBITMAP bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
            if(!bitmap)return false;
            auto* out=static_cast<unsigned char*>(pixels);
            for(std::size_t i=0;i<icon.rgba.size();i+=4){const unsigned a=icon.rgba[i+3];out[i]=(icon.rgba[i+2]*a+127)/255;out[i+1]=(icon.rgba[i+1]*a+127)/255;out[i+2]=(icon.rgba[i]*a+127)/255;out[i+3]=a;}
            std::vector<unsigned char> mask(((icon.width+15)/16)*2*icon.height,0);
            HBITMAP monochrome=CreateBitmap(icon.width,icon.height,1,1,mask.data());
            if(monochrome){ICONINFO data{};data.fIcon=TRUE;data.hbmColor=bitmap;data.hbmMask=monochrome;replacement=CreateIconIndirect(&data);DeleteObject(monochrome);}
            DeleteObject(bitmap);if(!replacement)return false;
        }
        const auto old=trayCustomIcon_;trayCustomIcon_=replacement;
        if(internalTrayVisible_ && !showInternalTrayIcon()){trayCustomIcon_=old;if(replacement)DestroyIcon(replacement);return false;}
        if(old)DestroyIcon(old);return true;
    }
    bool setTrayTooltip(const std::wstring& text) override {
        trayTooltip_ = text;
        return !internalTrayVisible_ || showInternalTrayIcon();
    }
    void setOnTrayAction(std::function<void(int)> callback) override { onTrayAction_ = std::move(callback); }

    bool notifyTray(const std::wstring& title, const std::wstring& message) override {
        if (!internalTrayVisible_) return false;
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data); data.hWnd = hwnd_; data.uID = kInternalTrayIconId;
        data.uFlags = NIF_INFO; data.dwInfoFlags = NIIF_INFO;
        wcsncpy_s(data.szInfoTitle, title.c_str(), _TRUNCATE);
        wcsncpy_s(data.szInfo, message.c_str(), _TRUNCATE);
        return Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
    }

    bool showInternalTrayIcon() {
        if (!hwnd_) {
            return false;
        }
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = hwnd_;
        data.uID = kInternalTrayIconId;
        data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
        data.uCallbackMessage = oneui::kTrayCallbackMessage;
        data.hIcon = trayCustomIcon_;
        if (!data.hIcon) data.hIcon = reinterpret_cast<HICON>(SendMessageW(hwnd_, WM_GETICON, ICON_SMALL, 0));
        if (!data.hIcon) data.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        if (!data.hIcon) {
            data.hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd_, GCLP_HICONSM));
        }
        if (!data.hIcon) {
            data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        }
        wcsncpy_s(data.szTip, (trayTooltip_.empty() ? options_.title : trayTooltip_).c_str(), _TRUNCATE);

        if (internalTrayVisible_) {
            if (Shell_NotifyIconW(NIM_MODIFY, &data)) return true;
            internalTrayVisible_ = false;
        }
        if (!Shell_NotifyIconW(NIM_ADD, &data)) {
            return false;
        }
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
        internalTrayVisible_ = true;
        return true;
    }

    void hideInternalTrayIcon() {
        if (!hwnd_ || !internalTrayVisible_) {
            return;
        }
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = hwnd_;
        data.uID = kInternalTrayIconId;
        Shell_NotifyIconW(NIM_DELETE, &data);
        internalTrayVisible_ = false;
    }

    void restoreFromTray() {
        if (!hwnd_) {
            return;
        }
        ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(hwnd_);
    }

    void showTrayMenu() {
        if (!hwnd_) {
            return;
        }
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }
        if (!trayMenu_.empty()) {
            // Capture IDs before entering the native menu loop: timer-driven
            // updates must not change the meaning of an already visible row.
            const auto items = trayMenu_;
            std::vector<std::pair<int,bool>> actions;
            const auto append=[&](const auto& self,HMENU target,const std::vector<NativeMenuItem>& entries,bool enabled)->void {
                for(const auto& item:entries){
                    if(item.separator){AppendMenuW(target,MF_SEPARATOR,0,nullptr);continue;}
                    const bool active=enabled && item.enabled;
                    std::wstring text;for(auto c:item.text){text+=c;if(c==L'&')text+=c;}auto shortcut=menuShortcutLabel(item.shortcut);if(!shortcut.empty())text+=L"\t"+shortcut;
                    MENUITEMINFOW info{};info.cbSize=sizeof(info);info.fMask=MIIM_STRING|MIIM_STATE|MIIM_FTYPE;
                    info.dwTypeData=text.data();info.fState=(active?MFS_ENABLED:MFS_DISABLED)|(item.checked?MFS_CHECKED:MFS_UNCHECKED);
                    info.fType=item.check==MenuCheck::Radio?MFT_RADIOCHECK:MFT_STRING;
                    if(!item.children.empty()){info.hSubMenu=CreatePopupMenu();if(!info.hSubMenu)continue;self(self,info.hSubMenu,item.children,active);info.fMask|=MIIM_SUBMENU;}
                    else {actions.emplace_back(item.id,active);info.wID=static_cast<UINT>(actions.size());info.fMask|=MIIM_ID;}
                    if(!InsertMenuItemW(target,GetMenuItemCount(target),TRUE,&info) && info.hSubMenu)DestroyMenu(info.hSubMenu);
                }
            };append(append,menu,items,true);
            const auto alive = callbackWindowAlive_;
            const auto callback = onTrayAction_;
            const auto handle = hwnd_;
            POINT point{}; GetCursorPos(&point); SetForegroundWindow(handle);
            const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                point.x, point.y, 0, handle, nullptr);
            if (alive->load()) PostMessageW(handle, WM_NULL, 0, 0);
            DestroyMenu(menu);
            if (alive->load() && command > 0 && command <= actions.size() && actions[command - 1].second && callback) {
                callback(actions[command - 1].first);
            }
            return;
        }
        const auto showText = oneui::builtinText(oneui::BuiltinText::ShowMainWindow);
        const auto quitText = oneui::builtinText(oneui::BuiltinText::QuitApplication);
        AppendMenuW(menu, MF_STRING, oneui::kTrayCommandShow, showText.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, oneui::kTrayCommandExit, quitText.c_str());
        SetMenuDefaultItem(menu, oneui::kTrayCommandShow, FALSE);
        POINT pt{};
        GetCursorPos(&pt);
        // 经典托盘菜单收尾：置前台并在弹出后补一条空消息，避免菜单不消失。
        SetForegroundWindow(hwnd_);
        const auto alive = callbackWindowAlive_;
        const auto handle = hwnd_;
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, handle, nullptr);
        if (alive->load()) PostMessageW(handle, WM_NULL, 0, 0);
        DestroyMenu(menu);
    }

    void minimize() override {
        ensureCreated();
        if (hwnd_) {
            ShowWindow(hwnd_, SW_MINIMIZE);
        }
    }
