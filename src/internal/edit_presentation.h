#pragma once

namespace oneui::internal {

// A native input dispatch can collect an editor's request to present the
// committed edit with its dependent view updates. Nested dispatches are separate.
class EditPresentationScope {
public:
    EditPresentationScope() : previous_(current_) { current_ = this; }
    ~EditPresentationScope() { current_ = previous_; }
    EditPresentationScope(const EditPresentationScope&) = delete;
    EditPresentationScope& operator=(const EditPresentationScope&) = delete;

    static void coalesce() {
        if (current_) current_->coalesced_ = true;
    }

    bool coalesced() const { return coalesced_; }

private:
    static inline thread_local EditPresentationScope* current_ = nullptr;
    EditPresentationScope* previous_;
    bool coalesced_ = false;
};

} // namespace oneui::internal
