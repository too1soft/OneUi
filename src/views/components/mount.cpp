#include "oneui/ui_declarative.h"
#include "registry.h"
#include <stdexcept>

namespace oneui::ui {
void Mount::setComponents(OneUiComponentRegistry *registry) {
    if(registry && (registry->thread!=std::this_thread::get_id() || registry->busy))
        throw std::invalid_argument("Component registry thread/callback");
    components_ = registry ? registry->value : nullptr;
}
}
