#pragma once

namespace oneui::internal {

// A native input dispatch can collect an editor's request to present the
// committed edit with its dependent view updates. Nested dispatches are separate.
class EditPresentationScope {
public:
    explicit EditPresentationScope(const void* window) : previous_(current_), window_(window) {
        current_ = this;
    }
    ~EditPresentationScope() { current_ = previous_; }
    EditPresentationScope(const EditPresentationScope&) = delete;
    EditPresentationScope& operator=(const EditPresentationScope&) = delete;

    static void coalesce() {
        if (current_) current_->coalesced_ = true;
    }

    static void notePostedWork(const void* window) {
        if (current_ && current_->coalesced_ && current_->window_ == window) current_->postedWork_ = true;
    }

    bool coalesced() const { return coalesced_; }
    bool hasPostedWork() const { return postedWork_; }

private:
    static inline thread_local EditPresentationScope* current_ = nullptr;
    EditPresentationScope* previous_;
    const void* window_;
    bool coalesced_ = false;
    bool postedWork_ = false;
};

} // namespace oneui::internal
