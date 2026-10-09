#include "page_factories.hpp"
#include <atomic>
#include <oneui/localization.h>
#include <oneui/oneui_c_api.h>
#include <thread>

namespace lab {
using namespace oneui;
namespace {
class FilesPage final : public ContentPage {
    Window &window_;
    std::shared_ptr<Label> result_;

  public:
    explicit FilesPage(Window &window) : window_(window) {
        auto input = std::make_shared<TextField>();
        input->setText(L"OneUI 剪贴板测试 · 你好");
        input->setClipboard(std::make_shared<SystemClipboard>());
        add(input);
        result_ = text(L"可向本窗口投递文件；只展示路径，不读取文件内容。");
        action(L"复制文本", [input, status = result_] {
            SystemClipboard{}.setText(input->text());
            status->setText(L"已复制到系统剪贴板");
        });
        action(L"粘贴文本", [input] { input->setText(SystemClipboard{}.text()); });
        action(L"系统文件选择器", [status = result_] {
            OneUiFileDialogOptionsUtf8 options{};
            options.mode = OneUiFileDialogOpenFile;
            std::vector<char> path(65536);
            std::size_t required = 0;
            const auto result = oneui_window_file_dialog_utf8(nullptr, &options, path.data(),
                                                              path.size(), &required);
            status->setText(
                result == 1   ? L"已选择文件 · UTF-8 路径字节数：" + std::to_wstring(required - 1)
                : result == 0 ? L"已取消"
                              : L"文件对话框未完成，返回码：" + std::to_wstring(result));
        });

    }
    void activate() override {
        window_.setFileDropHandler([status = result_](std::vector<std::wstring> paths, Point) {
            status->setText(paths.empty() ? L"空投递"
                                          : L"收到 " + std::to_wstring(paths.size()) +
                                                L" 个路径：" + paths.front());
        });
    }
    void deactivate() override { window_.setFileDropHandler({}); }
};
class ReactivePage final : public ContentPage {
    State<std::wstring> value_{L"共享状态"};
    std::shared_ptr<std::atomic<bool>> cancelled_;
    std::thread worker_;
    std::shared_ptr<Label> progress_;
    void cancel() {
        if (cancelled_)
            cancelled_->store(true);
        if (worker_.joinable())
            worker_.join();
    }

  public:
    explicit ReactivePage(Window &) {
        auto input = std::make_shared<TextField>();
        input->setText(value_.get());
        input->setOnChanged([this](const std::wstring &value) { value_.set(value); });
        add(input);
        auto first = text(L"");
        first->bindText(value_);
        auto second = text(L"");
        second->bindText(value_);
        progress_ = text(L"任务未运行");
        action(L"运行本地后台任务", [this] {
            cancel();
            cancelled_ = std::make_shared<std::atomic<bool>>(false);
            auto cancelled = cancelled_;
            std::weak_ptr<Label> progress = progress_;
            auto dispatcher = Application::current()->dispatcher();
            progress_->setText(L"正在处理…");
            worker_ = std::thread([cancelled, progress, dispatcher] {
                for (int i = 1; i <= 20 && !cancelled->load(); ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if (cancelled->load())
                        return;
                    if (!dispatcher([cancelled, progress, i] {
                            if (!cancelled->load())
                                if (auto label = progress.lock())
                                    label->setText(i == 20
                                                       ? L"任务完成"
                                                       : L"进度 " + std::to_wstring(i * 5) + L"%");
                        }))
                        return;
                }
            });
        });
        action(L"取消任务", [this] {
            cancel();
            progress_->setText(L"任务已取消");
        });

    }
    ~ReactivePage() override { cancel(); }
    void deactivate() override {
        cancel();
        progress_->setText(L"任务已停止；离开页面不会继续投递结果。");
    }
};
class ThemePage final : public ContentPage {
  public:
    ThemePage() {
        text(L"顶部主题与密度选项作用于全部页面。", 18);
        auto sample = text(L"内置文案：" + builtinText(BuiltinText::Accept));
        action(L"内置确认文案切换为中文", [sample] {
            setBuiltinText(BuiltinText::Accept, L"确认");
            sample->setText(builtinText(BuiltinText::Accept));
        });
        action(L"内置确认文案切换为英文", [sample] {
            setBuiltinText(BuiltinText::Accept, L"Accept");
            sample->setText(builtinText(BuiltinText::Accept));
        });
        action(L"恢复系统默认文案", [sample] {
            setBuiltinText(BuiltinText::Accept, std::nullopt);
            sample->setText(builtinText(BuiltinText::Accept));
        });

    }
};
class DiagnosticsPage final : public ContentPage {
    Window &window_;

  public:
    explicit DiagnosticsPage(Window &window) : window_(window) {
        text(L"使用 Tab / Shift+Tab 检查焦点顺序。", 18);
        auto input = std::make_shared<TextField>(L"诊断字段");
        input->setAccessibleName(L"诊断字段");
        add(input);
        auto report = std::make_shared<TextField>();
        report->setMultiline(true);
        report->setReadOnly(true);
        add(report, 280);
        action(L"启用开发诊断", [this] { window_.enableDeveloperTools(); });
        action(L"读取诊断快照", [this, report] {
            const auto data = developerReportJson(window_.developerSnapshot());
            report->setText(std::wstring(data.begin(), data.end()));
        });
        action(L"关闭诊断浮层", [this] {
            DeveloperOptions options;
            options.enabled = false;
            options.overlay = false;
            window_.setDeveloperTools(options);
        });

    }
};
} // namespace
std::unique_ptr<DemoPage> createUtilityPage(const PageDescriptor &page, Window &window) {
    if (page.kind == PageKind::Files)
        return std::make_unique<FilesPage>(window);
    if (page.kind == PageKind::Reactive)
        return std::make_unique<ReactivePage>(window);
    if (page.kind == PageKind::Theme)
        return std::make_unique<ThemePage>();
    return std::make_unique<DiagnosticsPage>(window);
}
} // namespace lab
