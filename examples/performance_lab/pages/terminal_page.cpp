#include "../terminal/workspace.hpp"
#include "page_factories.hpp"

namespace lab {
namespace {
class TerminalPage final : public DemoPage {
    terminal::Workspace workspace_;

  public:
    explicit TerminalPage(oneui::Window &window)
        : workspace_(window, oneui::Application::current()->dispatcher()) {}
    std::shared_ptr<oneui::Widget> widget() const override { return workspace_.root(); }
    void setTheme(bool dark, bool compact) override { workspace_.theme(dark, compact); }
    void activate() override { workspace_.activate(); }
    void deactivate() override { workspace_.deactivate(); }
};
} // namespace
std::unique_ptr<DemoPage> createTerminalPage(oneui::Window &window) {
    return std::make_unique<TerminalPage>(window);
}
} // namespace lab
