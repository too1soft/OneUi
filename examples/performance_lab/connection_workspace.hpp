#pragma once
#include "connection_styles.hpp"

namespace connection_demo {
class Workspace {
    Window &window_;
    std::shared_ptr<int> alive_ = std::make_shared<int>(0);
    std::unique_ptr<UiMailbox> mailbox_ = std::make_unique<UiMailbox>();
    bool active_ = true, closed_ = false;
    bool scheduled_ = false;
    Widget *beforePrompt_ = nullptr;
    bool wasForm_ = false, wasPrompt_ = false, wasDetail_ = false;
    bool wasGallery_ = false;
    std::unique_ptr<StyleWatcher> watcher_;
    void schedule() {
        if (!active_ || scheduled_)
            return;
        scheduled_ = true;
        auto weak = std::weak_ptr<int>(alive_);
        window_.requestAnimationFrame([this, weak](double) {
            if (weak.expired())
                return;
            scheduled_ = false;
            flush();
        });
    }
    void connectMailbox() {
        auto weak = std::weak_ptr<int>(alive_);
        mailbox_->setWake([this, weak] {
            window_.post([this, weak] {
                if (!weak.expired())
                    mailbox_->drain();
            });
        });
    }
    void watchStyles() {
        if (styles.dev)
            watcher_ = std::make_unique<StyleWatcher>(mailbox_->sender(), styles.files(),
                                                      [this] { styles.apply(); });
    }

  public:
    VM vm;
    Connections flow;
    Mount mount;
    Page page;
    StyleSession styles;
    std::unique_ptr<Samples> samples;
    std::unique_ptr<Element> galleryPage;
    Workspace(Window &window, std::function<void()> back, bool useTemplate = false,
              bool dev = false,
              std::filesystem::path css = std::filesystem::path(__FILE__).parent_path() /
                                          "connections.css")
        : window_(window), vm(mailbox_->sender()), flow(vm), mount([this] { schedule(); }),
          page(useTemplate ? buildTemplate(mount, vm, flow) : buildWorkspace(mount, vm, flow)),
          styles{mount, vm, flow, std::move(css), dev} {
        connectMailbox();
        flow.backToLab = std::move(back);
        flow.closeWindow = [this] { window_.close(); };
        mount.styles()->replace(declarativeTheme(vm.theme.get() == 1));
        mount.watch(vm.theme, [this](int) { styles.apply(); });
        mount.watch(vm.density, [this](int) { styles.apply(); });
        // Build this optional showcase once, only when first requested.
        mount.watch(flow.gallery, [this, useTemplate](bool shown) {
            if (shown && !galleryPage) {
                samples = std::make_unique<Samples>();
                galleryPage =
                    std::make_unique<Element>(buildGallery(mount, vm, *samples, !useTemplate));
                page.root.as<Stack>()->add(galleryPage->widget);
                page.root.as<Stack>()->setFlex(galleryPage->widget, galleryPage->flex);
                styles.visualPreset = [this] {
                    return samples->visualPreset.get() ? VisualPreset::Soft
                                                       : VisualPreset::Standard;
                };
                mount.watch(samples->visualPreset, [this](int) { styles.apply(); });
                styles.effectCss = [this] { return samples->effectCss(); };
                mount.watch(samples->strength, [this](int) { styles.apply(); });
                mount.watch(samples->timing, [this](int) { styles.apply(); });
                mount.watch(samples->gradient, [this](int) { styles.apply(); });
                styles.apply();
                samples->back.setAction([this] { flow.gallery.set(false); });
            }
            if (galleryPage)
                galleryPage->widget->setVisible(shown);
        });
        watchStyles();
        vm.focusError = [this](const std::string &field) {
            flush();
            window_.prepareLayoutSnapshot();
            window_.requestFocus(page.fields.at(field).get());
            revealField(page.root.widget, page.fields.at(field).get());
        };
    }
    ~Workspace() { close(); }
    void close() {
        suspend();
        closed_ = true;
    }
    void suspend() {
        if (!active_)
            return;
        active_ = false;
        mailbox_->close();
        alive_.reset();
        scheduled_ = false;
        watcher_.reset();
        if (vm.save.running.get()) {
            vm.save.running.set(false);
            vm.message.set(L"已暂停本地保存 · 草稿保留，返回后可重新保存。");
            if (vm.onSaveFailed)
                vm.onSaveFailed();
        }
        if (samples)
            samples->loading.set(false);
        mount.flush();
    }
    void resume() {
        if (active_ || closed_)
            return;
        // An old worker retains only the cancelled sender. It cannot complete
        // into the new page generation, even after a rapid leave/return.
        mailbox_ = std::make_unique<UiMailbox>();
        alive_ = std::make_shared<int>(0);
        vm.setTaskSender(mailbox_->sender());
        active_ = true;
        connectMailbox();
        watchStyles();
        schedule();
    }
    void flush() {
        if (!active_)
            return;
        const bool prompt = flow.prompt.get(), form = flow.form.get(),
                   detail = !flow.detailKey.get().empty();
        if (prompt && !wasPrompt_)
            beforePrompt_ = focusedField(page.root.widget);
        mount.flush();
        window_.prepareLayoutSnapshot();
        if (prompt && !wasPrompt_)
            window_.requestFocus(page.fields.at("$keep").get());
        else if (!prompt && wasPrompt_ && form == wasForm_ && detail == wasDetail_ && beforePrompt_)
            window_.requestFocus(beforePrompt_);
        else if (form != wasForm_ || detail != wasDetail_)
            window_.requestFocus(page.fields.at(form     ? "name"
                                                : detail ? "$detailEdit"
                                                         : "$table")
                                     .get());
        wasForm_ = form;
        wasPrompt_ = prompt;
        wasDetail_ = detail;
        if (flow.gallery.get() != wasGallery_) {
            window_.requestFocus(flow.gallery.get() ? mount.find("galleryBack").get()
                                                    : page.fields.at("$search").get());
            wasGallery_ = flow.gallery.get();
        }
    }
    void afterTab() {
        if (!active_)
            return;
        auto weak = std::weak_ptr<int>(alive_);
        window_.post([this, weak] {
            if (weak.expired())
                return;
            window_.prepareLayoutSnapshot();
            revealField(page.root.widget, focusedField(page.root.widget));
        });
    }
};
} // namespace connection_demo
