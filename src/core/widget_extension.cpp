#include "internal/widget_handle.h"
#include "oneui/native_interop.h"
#include <stdexcept>

namespace oneui {
struct Widget::ExtensionAttachment {
    std::shared_ptr<void> owner;
    std::function<void(const StyleSheet &, const StyleNode &)> style;
};
Widget::~Widget() { lifetime_.reset(); }
bool Widget::hasExtensionOwner() const { return bool(extension_); }
std::shared_ptr<Widget> retainNativeWidget(const OneUiWidget *handle) {
    return handle ? handle->widget : nullptr;
}
OneUiWidget *createNativeWidgetHandle(std::shared_ptr<Widget> widget) {
    if (!widget) throw std::invalid_argument("Native widget must not be null");
    auto result = std::make_unique<OneUiWidget>();
    result->widget = std::move(widget);
    return result.release();
}
void Widget::attachExtensionOwner(std::shared_ptr<void> owner,
        std::function<void(const StyleSheet &, const StyleNode &)> style) {
    if (!owner || extension_) throw std::invalid_argument("Widget already has an extension owner");
    extension_ = std::make_shared<ExtensionAttachment>(ExtensionAttachment{std::move(owner),std::move(style)});
}
void Widget::applyExtensionStyle(const StyleSheet &sheet, const StyleNode &node) {
    if (extension_ && extension_->style) extension_->style(sheet,node);
}
}
