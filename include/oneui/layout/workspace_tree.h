#pragma once
#include "oneui/layout/dock_model.h"
#include <string>
#include <unordered_map>

namespace oneui {
// Flat, language-neutral projection. No native child ownership or VM callbacks.
struct WorkspaceNode {
    std::string id, pane, first, second;
    DockAxis axis = DockAxis::Horizontal;
    float ratio = .5f;
};
struct WorkspaceGroup {
    std::string id, root;
};
struct WorkspacePane {
    std::string id;
    Size minimum;
};
struct WorkspaceRect {
    std::string key;
    Rect bounds;
};
struct WorkspaceGeometry {
    Size content;
    std::vector<WorkspaceRect> panes, dividers;
};
// Errors leave both the previous index and caller output untouched.
class ONEUI_API DockTreeIndex final {
  public:
    static constexpr size_t nodeLimit = 100000;
    DockError reset(std::vector<WorkspaceNode>, std::vector<WorkspaceGroup>,
                    std::vector<WorkspacePane>, float gap = 5);
    DockError layout(const std::string &group, Rect viewport, WorkspaceGeometry &,
                     const std::string &maximized = {}) const;
    const std::vector<WorkspaceNode> &nodes() const { return nodes_; }
    Size minimum(const std::string &group) const;

  private:
    static constexpr size_t missing = size_t(-1);
    struct Link {
        size_t first = missing, second = missing;
        Size minimum;
    };
    std::vector<WorkspaceNode> nodes_;
    std::vector<Link> links_;
    std::unordered_map<std::string, size_t> ids_, roots_;
    float gap_ = 5;
};
} // namespace oneui
