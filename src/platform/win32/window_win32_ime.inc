    bool imeTargetCurrent() const {
        const auto state = content_ ? content_->textInputState() : TextInputState{};
        return imeActive_ && state.editable && state.identity == imeIdentity_ && state.session == imeSession_;
    }
    void beginImeComposition() {
        const auto state = content_ ? content_->textInputState() : TextInputState{};
        imeActive_ = state.editable;
        imeIdentity_ = state.identity;
        imeSession_ = state.session;
        pendingHighSurrogate_ = 0;
    }
    void endImeComposition() {
        const bool clear = imeTargetCurrent();
        imeActive_ = false;
        const auto content = content_;
        if (clear && content) content->setTextComposition({}, 0);
    }
    void cancelImeComposition() {
        if (!imeActive_) return;
        const auto alive = callbackWindowAlive_;
        endImeComposition();
        if (!alive->load(std::memory_order_acquire)) return;
        if (hwnd_) {
            const auto window = hwnd_;
            const auto context = ImmGetContext(window);
            if (context) {
                ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
                ImmReleaseContext(window, context);
            }
        }
    }
    LRESULT dispatchImeComposition(WPARAM wParam, LPARAM flags) {
        if (!imeTargetCurrent()) { cancelImeComposition(); return 0; }
        const auto window = hwnd_;
        const auto context = ImmGetContext(window);
        if (!context) return 0;
        const auto read = [&](DWORD kind) {
            const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
            if (bytes <= 0 || bytes % sizeof(wchar_t) || bytes > 1024 * 1024) return std::wstring{};
            std::wstring result(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
            const LONG copied = ImmGetCompositionStringW(context, kind, result.data(), bytes);
            return copied == bytes ? result : std::wstring{};
        };
        const auto committed = flags & GCS_RESULTSTR ? read(GCS_RESULTSTR) : std::wstring{};
        const auto preedit = flags & GCS_COMPSTR ? read(GCS_COMPSTR) : std::wstring{};
        const LONG caret = ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0);
        ImmReleaseContext(window, context);
        const auto alive = callbackWindowAlive_;
        const auto content = content_;
        const bool clientPreedit = content->textInputState().drawsPreedit;
        if (flags & GCS_RESULTSTR) content->onTextCommitted(committed);
        if (!alive->load(std::memory_order_acquire)) return 0;
        if ((flags & GCS_COMPSTR) && imeTargetCurrent()) content->setTextComposition(preedit, static_cast<std::size_t>(std::max<LONG>(0, caret)));
        if (!alive->load(std::memory_order_acquire)) return 0;
        updateImePosition();
        if (!alive->load(std::memory_order_acquire)) return 0;
        requestRedraw();
        // RESULTSTR is already one atomic commit; DefWindowProc would emit duplicate WM_CHARs.
        return clientPreedit || (flags & GCS_RESULTSTR) ? 0 : DefWindowProcW(window, WM_IME_COMPOSITION, wParam, flags);
    }

    void flushDeferredImePosition() {
        if (imePositionAfterPaint_) {
            imePositionAfterPaint_ = false;
            updateImePosition();
        }
    }

    void updateImePosition(bool force = false) {
        const auto alive = callbackWindowAlive_;
        if (imeActive_ && !imeTargetCurrent()) cancelImeComposition();
        if (!alive->load(std::memory_order_acquire)) return;
        if (!hwnd_ || !content_ || !content_->visible()) {
            return;
        }

        const Rect logical = content_->textInputCaretRect();
        const float scale = normalizedDpiScale();
        const LONG left = static_cast<LONG>(std::lround(logical.x * scale));
        const LONG top = static_cast<LONG>(std::lround(logical.y * scale));
        const LONG right = static_cast<LONG>(std::lround((logical.x + logical.width) * scale));
        const LONG bottom = static_cast<LONG>(std::lround((logical.y + logical.height) * scale));

        // Key presses and every mouse event re-report the caret. Each report
        // is a round trip into the IME (milliseconds with some Chinese IMEs),
        // so only a moved caret or a new IME context sends one.
        const RECT caret{left, top, right, bottom};
        if (!force && imeRectSent_ && EqualRect(&caret, &lastImeRect_)) {
            return;
        }

        HIMC context = ImmGetContext(hwnd_);
        if (!context) {
            return;
        }
        lastImeRect_ = caret;
        imeRectSent_ = true;

        COMPOSITIONFORM composition{};
        composition.dwStyle = CFS_POINT;
        composition.ptCurrentPos = POINT{left, bottom};
        ImmSetCompositionWindow(context, &composition);

        CANDIDATEFORM candidate{};
        candidate.dwIndex = 0;
        candidate.dwStyle = CFS_EXCLUDE;
        candidate.ptCurrentPos = POINT{left, bottom};
        candidate.rcArea = RECT{left, top, std::max(left + 1, right), std::max(top + 1, bottom)};
        ImmSetCandidateWindow(context, &candidate);
        ImmReleaseContext(hwnd_, context);
    }
