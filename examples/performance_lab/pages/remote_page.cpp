#include "../remote_scene.hpp"
#include "page_factories.hpp"
#include <chrono>

namespace lab {
using namespace oneui;
namespace {
class RemotePage final : public ContentPage {
    Window &window_;
    RemoteScene scene_;
    std::shared_ptr<Label> status_;
    std::shared_ptr<Button> play_;
    std::shared_ptr<int> generation_;
    bool active_ = false;
    std::chrono::steady_clock::time_point lastFrame_;
    void status(const std::wstring &result = L"已提交完整帧") {
        status_->setText(result + L" · 帧 ID " + std::to_wstring(scene_.sequence()) + L"\n" +
                         L"640 × 360 · 缓冲 " + std::to_wstring(scene_.bytes() / 1024) +
                         L" KiB · " +
                         (scene_.ownedBuffer().expired() ? L"复制缓冲" : L"共享所有者仍存活"));
    }
    void submit() {
        scene_.submit(scene_.kind());
        status();
    }
    void schedule() {
        auto weak = std::weak_ptr<int>(generation_);
        window_.requestAnimationFrame([this, weak](double) {
            if (weak.expired() || !active_)
                return;
            const auto now = std::chrono::steady_clock::now();
            if (now - lastFrame_ >= std::chrono::milliseconds(33)) {
                lastFrame_ = now;
                submit();
            }
            schedule();
        });
    }
    void stop() {
        generation_.reset();
        if (play_)
            play_->setText(L"播放本地模拟帧");
    }
    std::shared_ptr<Select> choice(std::vector<std::wstring> items, int selected,
                                   std::function<void(int)> changed) {
        auto control = std::make_shared<Select>();
        control->setItems(std::move(items));
        control->setSelectedIndex(selected);
        control->setOnChanged(std::move(changed));
        add(control);
        return control;
    }

  public:
    explicit RemotePage(Window &window) : window_(window) {
        text(L"本地模拟 · 画面和输入不会发送到网络", 18);
        auto overlay = std::make_shared<OverlayHost>();
        overlay->setContent(scene_.frame);
        overlay->addOverlay(scene_.input);
        add(overlay, 320);
        status_ = text(L"");
        status_->setTextWrapping(true);
        body_->setFlex(status_, {0, 0, 64, 64});
        auto events = text(L"点击画面后输入文字；滚轮、组合键和坐标会显示在这里。");
        events->setTextWrapping(true);
        body_->setFlex(events, {0, 0, 76, 76});
        std::weak_ptr<RemoteInputRegion> weak = scene_.input;
        scene_.input->setOnPointer([weak, events](const RemotePointerEvent &event) {
            if (auto input = weak.lock())
                input->setRemoteCursorPosition(event.remotePosition);
            events->setText(L"坐标 " + std::to_wstring(int(event.remotePosition.x)) + L", " +
                            std::to_wstring(int(event.remotePosition.y)) + L" · 按钮 " +
                            std::to_wstring(int(event.button)) +
                            (event.pressed ? L" 按下" : L" 移动 / 释放") + L"\n滚轮 " +
                            std::to_wstring(event.wheelDeltaX) + L", " +
                            std::to_wstring(event.wheelDeltaY));
        });
        scene_.input->setOnRawKey([events](const RawKeyEvent &event) {
            events->setText(L"键码 " + std::to_wstring(event.virtualKey) + L" / 扫描码 " +
                            std::to_wstring(event.scanCode) +
                            (event.pressed ? L" 按下" : L" 释放") +
                            (event.repeat ? L" 重复" : L"") + L"\n" +
                            (event.ctrl ? L"Ctrl " : L"") + (event.alt ? L"Alt " : L"") +
                            (event.shift ? L"Shift " : L"") + (event.win ? L"系统键" : L""));
        });
        scene_.input->setOnTextInput(
            [events](const std::wstring &value) { events->setText(L"提交文字：" + value); });
        play_ = action(L"播放本地模拟帧", [this] {
            if (generation_) {
                stop();
                return;
            }
            if (!active_)
                return;
            generation_ = std::make_shared<int>(0);
            lastFrame_ = std::chrono::steady_clock::now();
            play_->setText(L"暂停模拟帧");
            schedule();
        });
        action(L"提交下一帧", [this] { submit(); });
        text(L"帧格式与所有权", 18);
        choice({L"RGBA · 复制", L"BGRA · 复制", L"RGBA · 共享所有者"}, 0, [this](int value) {
            if (value >= 0 && value <= 2) {
                scene_.submit(RemoteScene::FrameKind(value));
                status();
            }
        });
        action(L"提交 48 × 48 局部更新", [this] {
            status(scene_.damage() ? L"局部更新已接受" : L"当前格式不支持局部更新；原帧保留");
        });
        action(L"验证 NV12 拒绝结果", [this] {
            stop();
            status(scene_.probeNv12() ? L"NV12 请求被接受，需复核显示结果"
                                      : L"SDK 尚不支持 NV12；原帧保留");
        });
        text(L"缩放与远程光标", 18);
        choice({L"实际尺寸", L"完整显示", L"裁剪填满", L"拉伸"}, 1, [this](int value) {
            if (value < 0 || value > 3)
                return;
            scene_.frame->setScaleMode(ScaleMode(value));
            scene_.input->setScaleMode(RemoteInputScaleMode(value));
        });
        choice({L"默认光标", L"隐藏光标", L"位图十字光标"}, 0, [this](int value) {
            if (value >= 0 && value <= 2)
                scene_.cursor(RemoteCursorMode(value));
        });
        action(L"释放所有按键和鼠标按钮", [this, events] {
            scene_.input->releaseAllInputs();
            events->setText(L"已发送已按下输入的释放事件；离开页面也会自动释放。");
        });
        status();
    }
    ~RemotePage() override { deactivate(); }
    void activate() override { active_ = true; }
    void deactivate() override {
        active_ = false;
        stop();
        scene_.input->releaseAllInputs();
    }
};
} // namespace
std::unique_ptr<DemoPage> createRemotePage(Window &window) {
    return std::make_unique<RemotePage>(window);
}
} // namespace lab
