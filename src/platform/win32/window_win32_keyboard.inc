    void updateTrackedKeyState(WPARAM virtualKey, bool pressed) {
        if (virtualKey < trackedKeyState_.size()) {
            trackedKeyState_[static_cast<std::size_t>(virtualKey)] = pressed;
        }
    }

    bool trackedModifierDown(int genericKey, int leftKey, int rightKey) const {
        const auto down = [this](int virtualKey) {
            return virtualKey >= 0 &&
                   static_cast<std::size_t>(virtualKey) < trackedKeyState_.size() &&
                   trackedKeyState_[static_cast<std::size_t>(virtualKey)];
        };
        return down(genericKey) || down(leftKey) || down(rightKey);
    }

    void resetTrackedKeyState() {
        trackedKeyState_.fill(false);
    }

    KeyEvent makeKeyEvent(WPARAM wParam, LPARAM lParam, bool pressed) const {
        Key key = Key::Other;
        if (wParam == VK_TAB) {
            key = Key::Tab;
        } else if (wParam == VK_RETURN) {
            key = Key::Enter;
        } else if (wParam == VK_SPACE) {
            key = Key::Space;
        } else if (wParam == VK_BACK) {
            key = Key::Backspace;
        } else if (wParam == VK_LEFT) {
            key = Key::Left;
        } else if (wParam == VK_RIGHT) {
            key = Key::Right;
        } else if (wParam == VK_UP) {
            key = Key::Up;
        } else if (wParam == VK_DOWN) {
            key = Key::Down;
        } else if (wParam == VK_ESCAPE) {
            key = Key::Escape;
        } else if (wParam == VK_HOME) {
            key = Key::Home;
        } else if (wParam == VK_END) {
            key = Key::End;
        } else if (wParam == VK_PRIOR) {
            key = Key::PageUp;
        } else if (wParam == VK_NEXT) {
            key = Key::PageDown;
        } else if (wParam == VK_DELETE) {
            key = Key::Delete;
        } else if (wParam == VK_F2) {
            key = Key::F2;
        } else if (wParam == 'A') {
            key = Key::A;
        } else if (wParam == 'C') {
            key = Key::C;
        } else if (wParam == 'V') {
            key = Key::V;
        } else if (wParam == 'X') {
            key = Key::X;
        }

        KeyEvent event;
        event.key = key;
        event.shift = trackedModifierDown(VK_SHIFT, VK_LSHIFT, VK_RSHIFT) ||
                      (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.control = trackedModifierDown(VK_CONTROL, VK_LCONTROL, VK_RCONTROL) ||
                        (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        event.virtualKey = static_cast<unsigned int>(wParam);
        event.scanCode = static_cast<unsigned int>((lParam >> 16) & 0xff);
        event.pressed = pressed;
        event.repeat = pressed && ((lParam & (1LL << 30)) != 0);
        event.extended = (lParam & (1LL << 24)) != 0;
        event.alt = trackedModifierDown(VK_MENU, VK_LMENU, VK_RMENU) ||
                    (GetKeyState(VK_MENU) & 0x8000) != 0;
        event.win = trackedModifierDown(VK_LWIN, VK_LWIN, VK_RWIN) ||
                    (GetKeyState(VK_LWIN) & 0x8000) != 0 ||
                    (GetKeyState(VK_RWIN) & 0x8000) != 0;
        return event;
    }

    bool dispatchKeyDown(WPARAM wParam, LPARAM lParam) {
        suppressKeyText_ = false;
        const auto alive = callbackWindowAlive_;
        updateTrackedKeyState(wParam, true);
        KeyEvent event = makeKeyEvent(wParam, lParam, true);
        // Composition keys belong to the IME, not application commands or the
        // PTY. Committed text arrives atomically via GCS_RESULTSTR.
        if (imeTargetCurrent() || wParam == VK_PROCESSKEY) return false;
#if ONEUI_DEVELOPER_TOOLS
        if(developerTools_ && event.virtualKey==VK_F12 && !event.shift && !event.alt) {
            if(!event.repeat) {
                if(event.control)developerExportStatus_=exportDeveloperReport(L"oneui-devtools.json")?1:-1;
                else developerTools_->options.overlay=!developerTools_->options.overlay;
                requestRedraw();
            }
            return true;
        }
#endif
        const auto raw = rawKeyHandler_;
        const bool consumed = raw && raw(event);
        if (!alive->load(std::memory_order_acquire)) return true;
        if (consumed) {
            suppressKeyText_ = true;
            requestInteractiveRedraw();
            return true;
        }
        if (dispatchCommandKey(event)) {
            if (alive->load(std::memory_order_acquire)) {
                suppressKeyText_ = true;
                updateImePosition();
            }
            return true;
        }
        if (!alive->load(std::memory_order_acquire)) return true;
        if (!content_ || !content_->visible()) {
            return false;
        }
        const auto content = content_;
        if (content->onKeyDown(event)) {
            if (!alive->load(std::memory_order_acquire)) return true;
            requestInteractiveRedraw();
            updateImePosition();
            return true;
        }
        return false;
    }

    bool dispatchKeyUp(WPARAM wParam, LPARAM lParam) {
        const auto alive = callbackWindowAlive_;
        updateTrackedKeyState(wParam, false);
        KeyEvent event = makeKeyEvent(wParam, lParam, false);
        if (imeTargetCurrent() || wParam == VK_PROCESSKEY) return false;
        const auto raw = rawKeyHandler_;
        const bool consumed = raw && raw(event);
        if (!alive->load(std::memory_order_acquire)) return true;
        if (consumed) {
            requestInteractiveRedraw();
            return true;
        }
        if (!content_ || !content_->visible()) {
            return false;
        }
        const auto content = content_;
        if (content->onKeyUp(event)) {
            if (!alive->load(std::memory_order_acquire)) return true;
            requestInteractiveRedraw();
            return true;
        }
        return false;
    }

    void dispatchTextInput(WPARAM wParam) {
        if (suppressKeyText_) return;
        if (!content_ || !content_->visible()) {
            return;
        }

        const wchar_t character = static_cast<wchar_t>(wParam);
        if (character >= 0xD800 && character <= 0xDBFF) {
            pendingHighSurrogate_ = character;
            return;
        }

        std::wstring text;
        if (character >= 0xDC00 && character <= 0xDFFF) {
            if (pendingHighSurrogate_ != 0) {
                text.push_back(pendingHighSurrogate_);
                text.push_back(character);
                pendingHighSurrogate_ = 0;
            } else {
                text.push_back(L'\uFFFD');
            }
        } else {
            if (pendingHighSurrogate_ != 0) {
                text.push_back(L'\uFFFD');
                pendingHighSurrogate_ = 0;
            }
            if (character >= 32) {
                text.push_back(character);
            }
        }
        dispatchCommittedText(text);
    }

    void dispatchUnicodeScalar(std::uint32_t scalar) {
        if (scalar < 32 || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF)) {
            return;
        }
        std::wstring text;
        if (scalar <= 0xFFFF) {
            text.push_back(static_cast<wchar_t>(scalar));
        } else {
            scalar -= 0x10000;
            text.push_back(static_cast<wchar_t>(0xD800 + (scalar >> 10)));
            text.push_back(static_cast<wchar_t>(0xDC00 + (scalar & 0x3FF)));
        }
        dispatchCommittedText(text);
    }

    void dispatchCommittedText(const std::wstring& text) {
        if (text.empty() || !content_ || !content_->visible()) {
            return;
        }
        const auto alive = callbackWindowAlive_;
        const auto content = content_;
        if (content->onTextCommitted(text) && alive->load(std::memory_order_acquire)) {
            requestInteractiveRedraw();
        }
    }
