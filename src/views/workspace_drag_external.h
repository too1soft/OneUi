#pragma once
#include "runtime.h"
namespace oneui::views {
// Native shell chrome can join an existing declarative drag domain. Geometry,
// capture lifecycle, preview and intent generation stay in the WP4 coordinator.
void registerWorkspaceTabs(Runtime&, const std::string& domain, const std::string& id,
                           const std::shared_ptr<Widget>&);
void workspaceTabDrag(Runtime&, const std::string& domain, const std::string& source,
                      int phase, Point);
}
