#pragma once
#include "demo_catalog.hpp"
#include <memory>
#include <oneui/oneui.h>
#include <oneui/ui_theme.h>
#include <string>

namespace lab {
void applyDemoButton(oneui::Button &, const oneui::StyleSheet &, bool selected = false);
class DemoPage {
  public:
    virtual ~DemoPage() = default;
    virtual std::shared_ptr<oneui::Widget> widget() const = 0;
    virtual void activate() {}
    virtual void deactivate() {}
    virtual void applicationActivated(const oneui::ApplicationActivation &) {}
    virtual void setTheme(bool dark, bool compact) {
        (void)dark;
        (void)compact;
    }
    virtual bool key(const oneui::KeyEvent &) { return false; }
};
// A page owns its commands, bindings and callbacks; the shell owns navigation only.
class ContentPage : public DemoPage {
  public:
    ContentPage();
    std::shared_ptr<oneui::Widget> widget() const override { return scroll_; }
    void setTheme(bool dark, bool compact) override;
    std::shared_ptr<oneui::Label> text(std::wstring value, float size = 14);
    std::shared_ptr<oneui::Button> action(std::wstring title, std::function<void()> callback);
    void add(const std::shared_ptr<oneui::Widget> &widget, float height = 40);

  protected:
    std::shared_ptr<oneui::Stack> body_;
    std::shared_ptr<oneui::ScrollView> scroll_;
    std::shared_ptr<oneui::StyleSheet> styles_;
    std::vector<std::shared_ptr<oneui::Label>> labels_;
};
std::unique_ptr<DemoPage> createDemoPage(const PageDescriptor &, oneui::Window &);
} // namespace lab
