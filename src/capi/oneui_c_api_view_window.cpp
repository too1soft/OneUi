#include "oneui_c_api_view_internal.h"
#include <mutex>
namespace oneui::capi::views {
namespace {
std::mutex mountsMutex;
std::map<OneUiWindow *, std::vector<std::weak_ptr<Mount>>> mounts;
} // namespace
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
