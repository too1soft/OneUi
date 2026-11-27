#pragma once

#include <utility>

namespace oneui {
class Widget;

namespace internal {

// Marks only the structural notification from publishing rows of the virtual
// table currently being painted inside its scroll clip.
struct VirtualRowPaint {
    static inline thread_local bool fullClientPaint = false;
    static inline thread_local bool resizeOverlay = false;
    static inline thread_local const Widget* clippedContents = nullptr;
    static inline thread_local const Widget* layoutForPaint = nullptr;
    static inline thread_local const Widget* publishingRows = nullptr;
    static inline thread_local const Widget* commit = nullptr;
    static inline thread_local const void* damageOwner = nullptr;

    template<class T>
    class Scope {
    public:
        Scope(T& slot, T value) noexcept : slot_(slot), previous_(std::exchange(slot, value)) {}
        ~Scope() { slot_ = previous_; }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        T& slot_;
        T previous_;
    };
};

} // namespace internal
} // namespace oneui
