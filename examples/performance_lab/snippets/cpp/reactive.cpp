#include <oneui/application.h>
#include <oneui/controls/label.h>

struct ExampleState {
    oneui::State<std::wstring> name{L"OneUI"};
    std::shared_ptr<oneui::Label> label = std::make_shared<oneui::Label>();
    ExampleState() { label->bindText(name); }
};
// Safe to retain on a worker; callback captures no raw Window pointer.
bool example_post(const oneui::Application::Dispatcher &dispatch, std::weak_ptr<ExampleState> owner,
                  std::wstring result) {
    return dispatch([owner, result = std::move(result)] {
        if (auto state = owner.lock())
            state->name.set(result);
    });
}
