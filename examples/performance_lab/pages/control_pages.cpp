#include "internal/unicode.h"
#include "page_factories.hpp"
#include <atomic>
#include <oneui/controls/tree_view.h>
#include <oneui/fonts.h>
#include <oneui/style_adapter.h>
#include <oneui/ui_native_theme.h>
#include <thread>

namespace lab {
using namespace oneui;
class TextPage final : public ContentPage {
    struct FontJob {
        std::atomic<bool> cancelled{false}, completed{false};
    };
    std::shared_ptr<FontJob> fontJob_;
    std::thread fontWorker_;
    std::shared_ptr<std::vector<FontFamilyInfo>> fonts_ =
        std::make_shared<std::vector<FontFamilyInfo>>();
    std::shared_ptr<Select> fontChoice_;
    std::shared_ptr<Label> fontStatus_;
    std::shared_ptr<std::string> selectedFamily_ = std::make_shared<std::string>();
    std::function<void(const std::string &)> applyFont_;
    void stopFonts() {
        if (fontJob_) {
            if (!fontJob_->completed.load() && !fontJob_->cancelled.load())
                fontStatus_->setText(L"字体扫描已停止；保留已有选择，可再次刷新。");
            fontJob_->cancelled = true;
        }
        if (fontWorker_.joinable())
            fontWorker_.join();
    }
    void scanFonts() {
        stopFonts();
        fontJob_ = std::make_shared<FontJob>();
        fontStatus_->setText(L"正在后台读取系统字体…");
        auto dispatch = Application::current()->dispatcher();
        fontWorker_ =
            std::thread([job = fontJob_, fonts = fonts_, selected = selectedFamily_,
                         apply = applyFont_, dispatch, choice = std::weak_ptr<Select>(fontChoice_),
                         status = std::weak_ptr<Label>(fontStatus_)] {
                try {
                    auto snapshot = enumerateSystemFonts();
                    if (job->cancelled.load())
                        return;
                    dispatch([job, fonts, selected, apply, choice, status,
                              snapshot = std::move(snapshot)]() mutable {
                        if (job->cancelled.load())
                            return;
                        auto select = choice.lock();
                        auto label = status.lock();
                        if (!select || !label)
                            return;
                        *fonts = std::move(snapshot);
                        std::vector<std::wstring> items{L"使用系统默认字体"};
                        std::size_t mono = 0;
                        for (const auto &font : *fonts) {
                            items.push_back(unicode::fromUtf8(font.family) +
                                            (font.monospaced ? L" · 等宽" : L""));
                            if (font.monospaced)
                                ++mono;
                        }
                        const auto retainedFamily = *selected;
                        int selection = 0;
                        for (std::size_t i = 0; i < fonts->size(); ++i)
                            if (fonts->at(i).family == retainedFamily)
                                selection = int(i + 1);
                        select->setItems(std::move(items));
                        select->setSelectedIndex(selection);
                        *selected = selection ? retainedFamily : std::string{};
                        apply(*selected);
                        label->setText(L"系统字体 " + std::to_wstring(fonts->size()) +
                                       L" 个；其中等宽 " + std::to_wstring(mono) +
                                       L" 个。选择后应用于下方输入字段。");
                        job->completed = true;
                    });
                } catch (const std::exception &error) {
                    dispatch([job, status, message = unicode::fromUtf8(error.what())] {
                        if (!job->cancelled.load()) {
                            if (auto label = status.lock())
                                label->setText(L"字体枚举未完成：" + message);
                            job->completed = true;
                        }
                    });
                }
            });
    }

  public:
    TextPage() {
        text(L"试试中文输入、Emoji、选择、撤销和复制粘贴。", 18);
        fontStatus_ = text(L"按需读取本机安装字体；不会自动扫描或下载字体。");
        fontStatus_->setTextWrapping(true);
        body_->setFlex(fontStatus_, {0, 0, 48, 48});
        action(L"扫描 / 刷新系统字体", [this] { scanFonts(); });
        fontChoice_ = std::make_shared<Select>();
        fontChoice_->setItems({L"使用系统默认字体"});
        add(fontChoice_);
        auto status = text(L"输入状态");
        auto input = std::make_shared<TextField>(L"输入文字");
        input->setClipboard(std::make_shared<SystemClipboard>());
        input->setText(L"你好 OneUI · 👩‍💻 · العربية");
        input->setOnChanged([status](const std::wstring &value) {
            status->setText(L"字符单元数：" + std::to_wstring(value.size()));
        });
        add(input);
        auto multiline = std::make_shared<TextField>(L"多行输入");
        multiline->setMultiline(true);
        multiline->setClipboard(std::make_shared<SystemClipboard>());
        multiline->setText(
            L"保留光标、选择与滚动位置。\n切换页面后再返回，内容仍然存在。\n可以继续输入。\n");
        add(multiline, 220);
        auto password = std::make_shared<TextField>(L"密码字段");
        password->setPasswordMode(true);
        add(password);
        applyFont_ = [input, multiline, password](const std::string &name) {
            const auto family = unicode::fromUtf8(name);
            input->setFontFamily(family);
            multiline->setFontFamily(family);
            password->setFontFamily(family);
        };
        fontChoice_->setOnChanged(
            [fonts = fonts_, selected = selectedFamily_, apply = applyFont_](int index) {
                if (index < 0 || std::size_t(index) > fonts->size())
                    return;
                *selected = index == 0 ? std::string{} : fonts->at(index - 1).family;
                apply(*selected);
            });
        action(L"切换只读", [input] { input->setReadOnly(!input->readOnly()); });
        action(L"撤销输入", [input] { input->undo(); });
        action(L"重做输入", [input] { input->redo(); });
        auto wrap = std::make_shared<Label>(L"长文本会在原生文字布局中测量；中文、拉丁文字与双向文"
                                            L"字共享同一套排版和命中结果。缩窄窗口观察换行。");
        wrap->setTextWrapping(true);
        add(wrap, 100);
    }
    ~TextPage() override { stopFonts(); }
    void deactivate() override { stopFonts(); }
};
class DataPage final : public ContentPage {
    std::shared_ptr<Table> table_ = std::make_shared<Table>();
    std::vector<std::vector<std::wstring>> rows_;

  public:
    DataPage() {
        auto status = text(L"表格：选择、双击和拖动重排");
        table_->setColumns({{L"ID", 80}, {L"模拟节点", 0}, {L"状态", 100}});
        for (int i = 0; i < 1000; ++i)
            rows_.push_back({std::to_wstring(i + 1), L"节点 " + std::to_wstring(i + 1),
                             i % 3 ? L"在线" : L"离线"});
        table_->setRows(rows_);
        table_->setReorderEnabled(true);
        table_->setOnChanged(
            [status](int index) { status->setText(L"选中行：" + std::to_wstring(index)); });
        table_->setOnActivated(
            [status](int index) { status->setText(L"激活行：" + std::to_wstring(index)); });
        table_->setOnReorderRequested([this](int from, int to) {
            if (from < 0 || to < 0 || from >= int(rows_.size()) || to >= int(rows_.size()))
                return;
            auto row = rows_[from];
            rows_.erase(rows_.begin() + from);
            rows_.insert(rows_.begin() + to, row);
            table_->setRows(rows_);
            table_->setSelectedIndex(to);
        });
        add(table_, 320);
        text(L"TreeView · 用方向键展开、收起和选择", 18);
        auto tree = std::make_shared<TreeView>();
        tree->setItems({{L"root", L"", L"演示资源", L"3 个分组", true},
                        {L"a", L"root", L"布局", L"", true},
                        {L"b", L"root", L"性能", L"", true},
                        {L"c", L"root", L"功能", L"", true},
                        {L"c1", L"c", L"原生菜单", L"叶子节点", true}});
        tree->setOnSelectionChanged(
            [status](const std::wstring &id) { status->setText(L"树节点 ID：" + id); });
        add(tree, 220);
        auto list = std::make_shared<List>();
        list->setItems(
            {{L"列表一", L"使用原生选择"}, {L"列表二", L"键盘可达"}, {L"列表三", L"保留状态"}});
        add(list, 180);
    }
};
std::unique_ptr<DemoPage> createControlPage(const PageDescriptor &page, Window &) {
    if (page.kind == PageKind::Text)
        return std::make_unique<TextPage>();
    if (page.kind == PageKind::Data)
        return std::make_unique<DataPage>();
    return createBasicControlsPage();
}
} // namespace lab
