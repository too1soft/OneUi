#pragma once
#include "oneui/mobile/appearance.h"
#include "oneui/mobile/text_input.h"
#include "oneui/mobile/widget_host.h"
#include <deque>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace oneui::mobile {
struct ApplicationRequest {
    uint64_t id = 0;
    std::string action;
    std::string payload;
};

// UI-thread request queue. Native hosts execute services off the UI thread and
// deliver results on it. Opaque payloads are never logged or persisted here.
class ApplicationServices final {
  public:
    uint64_t request(std::string action, std::string payload = {});
    bool take(ApplicationRequest &request);
    // Owner-thread deadlines let native event loops sleep without painting.
    void wakeAfter(int milliseconds);
    int wakeDelayMillis() const;
    bool consumeWakeup();
  private:
    uint64_t next_ = 0;
    size_t bytes_ = 0;
    std::deque<ApplicationRequest> pending_;
    std::chrono::steady_clock::time_point wake_{};
};

class Application {
  public:
    virtual ~Application() = default;
    virtual std::shared_ptr<Widget> root() const = 0;
    virtual void viewport(Size) {}
    virtual void active(bool) {}
    virtual bool back() { return false; }
    // Structural changes are deferred until input/paint callbacks have ended.
    virtual bool hasPending() const { return false; }
    virtual void flush() {}
    virtual void reply(uint64_t, int, const std::string &) {}
    virtual void appearanceChanged(const OneUiMobileAppearanceStateV1 &) {}
    virtual void typography(float, float) {}
    Appearance &appearance() { return appearance_; }
  private:
    Appearance appearance_;
};

// Shared widget/input/application lifetime. Android, UIKit and HarmonyOS hosts
// supply surfaces and OS services; applications supply their OneUI widget tree.
class ApplicationHost final {
  public:
    using Factory = std::function<std::unique_ptr<Application>(ApplicationServices &)>;
    explicit ApplicationHost(const Factory &factory);
    ~ApplicationHost();
    ApplicationHost(const ApplicationHost &) = delete;
    ApplicationHost &operator=(const ApplicationHost &) = delete;
    Runtime &runtime() { return widgets_.runtime(); }
    WidgetHost &widgets() { return widgets_; }
    TextInputSession &input() { return *input_; }
    Application &application() { return *application_; }
    ApplicationServices &services() { return services_; }
    OneUiMobileResult lifecycle(int state);
    OneUiMobileResult viewport(const OneUiMobileViewportV1 &viewport);
    OneUiMobileResult surface(bool available);
    OneUiMobileResult pointer(const OneUiMobilePointerV1 &pointer);
    OneUiMobileResult cancelPointers();
    OneUiMobileResult paint(const std::function<Canvas *()> &canvas, uint64_t timeNs);
    void reply(uint64_t id, int code, const std::string &payload);
    void flush();
    bool back();
    void syncAppearance();
  private:
    ApplicationServices services_;
    WidgetHost widgets_;
    std::unique_ptr<Application> application_;
    std::unique_ptr<TextInputSession> input_;
    uint64_t appearanceRevision_ = 0;
    bool active_ = false;
    void activate();
};
} // namespace oneui::mobile

// The product defines this factory in its widget module. Generic native host
// code is linked from the prebuilt OneUI SDK, never copied into that product.
#if defined(__GNUC__) || defined(__clang__)
#define ONEUI_MOBILE_FACTORY_EXPORT __attribute__((visibility("default")))
#else
#define ONEUI_MOBILE_FACTORY_EXPORT
#endif
extern "C" ONEUI_MOBILE_FACTORY_EXPORT oneui::mobile::Application *oneui_create_mobile_application(oneui::mobile::ApplicationServices *services);
