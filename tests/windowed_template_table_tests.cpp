#include "oneui/canvas.h"
#include "oneui/controls/template_table.h"
#include "internal/table_layout_turn.h"

#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
using namespace oneui;

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

bool equal(float first, float second) {
    return std::fabs(first - second) < 0.01f;
}

class RowProbe final : public Widget {
public:
    explicit RowProbe(size_t index) : index(index) {}
    void paint(Canvas&) override {}
    bool isFocusable() const override { return true; }
    bool onMouseDown(const MouseEvent&) override { return true; }
    bool onMouseUp(const MouseEvent&) override { ++ups; return true; }
    size_t index;
    int ups = 0;
};

struct Fixture {
    std::vector<std::string> keys;
    std::vector<float> heights;
    std::shared_ptr<RowProbe> pressed;
    int mounted = 0;
    int retired = 0;
    std::shared_ptr<RowProbe> header = std::make_shared<RowProbe>(0);
    std::shared_ptr<VirtualTemplateList> rows = std::make_shared<VirtualTemplateList>();
    TemplateTable table{header, rows, true};

    explicit Fixture(bool layout = true) {
        for (int i = 0; i < 50; ++i) {
            keys.push_back(std::to_string(i));
            heights.push_back(44.0f);
        }
        heights[0] = 84;
        heights[1] = heights[2] = 0;
        heights[24] = 84;
        rows->setTableInsets({4, 13, 0, 16});
        rows->setFactory([&](size_t i, const std::string&) {
            ++mounted;
            auto row = std::make_shared<RowProbe>(i);
            if (i == 0) pressed = row;
            return row;
        }, [&](const std::string&) { ++retired; });
        rows->setTableRows(keys, heights, false);
        table.setHeaderHeight(30);
        table.setMinimumWidth(500);
        table.setContentWidth(500);
        table.setContentHeight(4 + std::accumulate(heights.begin(), heights.end(), 0.0f));
        table.setWheelStep(44);
        table.setFrame({10, 20, 320, 220});
        if (layout) table.layoutSubtree();
    }
};

void testGeometryAndPointer() {
    Fixture f;
    require(f.mounted >= 4 && f.mounted <= 8, "initial viewport must construct a bounded range");
    require(f.rows->rowFrame("1").height == 0 && f.rows->rowFrame("2").height == 0,
            "collapsed rows must not render");
    const auto first = f.rows->rowFrame("0");
    require(equal(first.x, f.rows->frame().x + 16) && equal(first.width, f.rows->frame().width - 29),
            "table body insets must survive windowing");
    require(equal(first.y, f.header->frame().y + 34) && equal(first.height, 84),
            "group heading must occupy its authoritative height");
    require(equal(f.rows->rowFrame("3").y, first.y + first.height), "collapsed rows must consume zero height");
    const auto headerY = f.header->frame().y;
    const auto headerX = f.header->frame().x;
    f.table.setHorizontalScrollOffset(30);
    f.table.layoutSubtree();
    require(equal(f.header->frame().x, headerX - 30) &&
            equal(f.rows->rowFrame("0").x, first.x - 30), "header and rows must scroll horizontally together");
    const auto hit = f.rows->rowFrame("0");
    MouseEvent down{Point{hit.x + 50, hit.y + 10}, MouseButton::Left};
    require(f.table.onMouseDown(down), "visible row must receive pointer press");
    f.table.setScrollOffset(900);
    f.table.layoutSubtree();
    require(equal(f.header->frame().y, headerY), "vertical scrolling must leave the header pinned");
    require(f.rows->rowFrame("0").height == 84, "pressed row must remain mounted outside viewport");
    require(f.table.onMouseUp(down) && f.pressed->ups == 1, "pointer release must reach its pressed row");
    require(f.retired > 0 && f.rows->mountedCount() <= 10, "scrolling should retire unused rows");
    f.table.setScrollOffset(f.table.maxScrollOffset());
    f.table.layoutSubtree();
    require(f.rows->rowFrame("49").height == 44, "jump to the end must mount the final row");
}

void testKeyboardAndWheel() {
    Fixture f;
    require(f.rows->focusFirstLeaf(), "focus should enter the first row");
    KeyEvent tab{};
    tab.key = Key::Tab;
    require(f.rows->onKeyDown(tab), "Tab should move past collapsed rows");
    const auto focused = [&] {
        return std::dynamic_pointer_cast<RowProbe>(f.rows->activeFocusChild());
    };
    require(focused() && focused()->index == 3, "focus should skip zero-height rows");
    for (size_t next = 4; next <= 11; ++next) {
        require(f.rows->onKeyDown(tab) && focused() && focused()->index == next,
                "Tab should cross the initial mounted range in order");
    }
    require(f.rows->focusLastLeaf() && focused() && focused()->index == 49,
            "backward entry into the table must target its final row");
    tab.shift = true;
    require(f.rows->onKeyDown(tab) && focused() && focused()->index == 48,
            "Shift+Tab should remount the preceding row");
    const MouseWheelEvent wheel{{100, 100}, -1.0f, false, false, false, 1000.0};
    require(f.table.onMouseWheel(wheel), "table should accept the wheel delta");
    require(equal(f.table.scrollOffset(), 0), "wheel should use the existing scroll animation");
    f.table.tickAnimations(1050);
    const auto intermediate = f.table.scrollOffset();
    require(intermediate > 0.0f && intermediate < 44.0f, "wheel motion should pass through an intermediate offset");
    f.table.tickAnimations(1400);
    require(equal(f.table.scrollOffset(), 44), "one wheel delta should retain the configured step");
}

void testTemporarilyEmptyViewport() {
    Fixture f;
    const auto normal = f.table.frame();
    const auto initial = f.rows->mountedCount();
    const auto first = f.pressed;
    const int created = f.mounted;
    f.table.setFrame({normal.x, normal.y, 0, 0});
    f.table.layoutSubtree();
    require(f.rows->mountedCount() == initial && f.retired == 0, "zero viewport must preserve realized rows");
    f.table.setFrame(normal);
    f.table.layoutSubtree();
    require(f.mounted == created && f.pressed == first, "restoring a table must reuse its rows");

    f.table.setFrame({normal.x, normal.y, 0, 0});
    f.table.layoutSubtree();
    f.keys.erase(f.keys.begin() + 3);
    f.heights.erase(f.heights.begin() + 3);
    f.rows->setTableRows(f.keys, f.heights, false);
    f.table.layoutSubtree();
    require(f.retired == 1 && f.rows->rowFrame("3").height == 0,
            "a viewport without drawable area must still retire removed keys");
}

void testStartupRowPreparation() {
    using internal::TableLayoutTurn;
    Fixture f(false);
    {
        TableLayoutTurn turn(std::chrono::milliseconds(0));
        f.table.layoutSubtree();
        require(turn.deferred() && f.mounted == 1 && f.rows->children().empty(),
                "an exhausted preparation turn must leave its first row detached");
    }
    {
        TableLayoutTurn turn(std::optional<Point>{{0, 0}});
        f.table.layoutSubtree();
        require(turn.deferred() && f.mounted == 1 && f.rows->children().empty(),
                "input outside an unpublished table must leave preparation pending");
    }
    {
        TableLayoutTurn turn(std::optional<Point>{{100, 100}});
        f.table.layoutSubtree();
        require(!turn.deferred() && f.rows->children().size() == static_cast<size_t>(f.mounted),
                "input within the viewport must get a complete hit-test tree");
        const auto first = f.rows->rowFrame("0");
        require(first.height == 84 && f.table.onMouseDown({{first.x+50, first.y+10}, MouseButton::Left}),
                "a row prepared before the input turn must receive the press");
    }
    Fixture retry(false);
    {
        TableLayoutTurn first(std::chrono::milliseconds(0));
        retry.table.layoutSubtree();
    }
    retry.keys.erase(retry.keys.begin());
    retry.heights.erase(retry.heights.begin());
    retry.rows->setTableRows(retry.keys, retry.heights, false);
    retry.table.layoutSubtree();
    require(retry.retired == 1 && retry.rows->rowFrame("0").height == 0,
            "refreshing the model must discard a removed detached row");
}
}

int main() {
    try {
        testGeometryAndPointer();
        testKeyboardAndWheel();
        testTemporarilyEmptyViewport();
        testStartupRowPreparation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "windowed table geometry, capture, focus, scrolling and restore passed\n";
    return 0;
}
