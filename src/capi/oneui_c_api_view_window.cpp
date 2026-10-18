#include "oneui_c_api_view_internal.h"
#include <mutex>
namespace oneui::capi::views {
namespace {
std::mutex mountsMutex;
std::map<OneUiWindow *, std::vector<std::weak_ptr<Mount>>> mounts;
struct ChildDestination {
    std::weak_ptr<Widget> widget;
    std::weak_ptr<Mount> mount;
};
std::vector<ChildDestination> childDestinations;
} // namespace
void reserveChildDestination(const std::shared_ptr<Widget> &widget, const std::shared_ptr<Mount> &mount) {
    std::lock_guard<std::mutex> lock(mountsMutex);
    childDestinations.erase(std::remove_if(childDestinations.begin(), childDestinations.end(),
        [](const auto &entry) { return entry.widget.expired() || entry.mount.expired(); }),
        childDestinations.end());
    for (const auto &entry : childDestinations)
        check(entry.widget.lock() != widget, ONEUI_VIEW_BUSY, "Destination Panel already reserved");
    childDestinations.push_back({widget, mount});
}
void releaseChildDestination(const std::shared_ptr<Widget> &widget, const Mount *mount) {
    std::lock_guard<std::mutex> lock(mountsMutex);
    childDestinations.erase(std::remove_if(childDestinations.begin(), childDestinations.end(),
        [&](const auto &entry) {
            return entry.mount.expired() ||
                (entry.widget.lock() == widget && entry.mount.lock().get() == mount);
        }), childDestinations.end());
}
void registerMount(OneUiWindow *window, const std::shared_ptr<Mount> &mount) {
    std::lock_guard<std::mutex> lock(mountsMutex);
    auto &entries = mounts[window];
    entries.erase(
        std::remove_if(entries.begin(), entries.end(), [](auto &weak) { return weak.expired(); }),
        entries.end());
    entries.push_back(mount);
}
bool closeWindowMounts(OneUiWindow *window) noexcept {
    return call("oneui_window_destroy", [&] {
               std::vector<std::weak_ptr<Mount>> entries;
               {
                   std::lock_guard<std::mutex> lock(mountsMutex);
                   auto found = mounts.find(window);
                   if (found == mounts.end())
                       return;
                   for (auto &weak : found->second)
                       if (auto mount = weak.lock())
                           check(mount->thread == std::this_thread::get_id(),
                                 ONEUI_VIEW_WRONG_THREAD, "Window destroy requires view UI thread");
                   entries.swap(found->second);
                   mounts.erase(found);
               }
               for (auto &weak : entries)
                   if (auto mount = weak.lock())
                       mount->close();
           }) == ONEUI_VIEW_OK;
}
} // namespace oneui::capi::views
