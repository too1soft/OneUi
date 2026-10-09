#include <oneui/oneui.h>

std::shared_ptr<oneui::SplitView> example_split() {
    auto split = std::make_shared<oneui::SplitView>();
    split->setFirst(std::make_shared<oneui::TreeView>());
    split->setSecond(std::make_shared<oneui::TextField>());
    split->setResizable(true);
    split->setMinimumPaneExtent(120, 240);
    split->setSplitRatio(.25f);
    split->setOnSplitRatioCommitted([](float ratio) {
        // Persist a committed ratio, rather than every pointer movement.
        (void)ratio;
    });
    return split;
}
