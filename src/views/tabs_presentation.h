#pragma once
#include "oneui/ui_keyed_tabs.h"

namespace oneui::ui::detail {
// The existing shared_ptr control block keeps per-control authoring state across
// Element copies without changing any public class layout. This named type must
// be identical in the factory, property and reload translation units.
struct TabsPresentation {
    bool document = true;
    std::optional<TabsSizingMode> sizing;
    void operator()(KeyedTabs* tabs) const noexcept { delete tabs; }
};

inline std::shared_ptr<KeyedTabs> makeAuthoredTabs(const std::shared_ptr<Tabs>& tabs) {
    // shared_ptr invokes the supplied deleter if control-block allocation fails.
    return std::shared_ptr<KeyedTabs>(new KeyedTabs(tabs), TabsPresentation{});
}

inline TabsPresentation* tabsPresentation(const std::shared_ptr<KeyedTabs>& tabs) {
    return std::get_deleter<TabsPresentation>(tabs);
}
} // namespace oneui::ui::detail
