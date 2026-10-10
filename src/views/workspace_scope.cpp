#include "runtime.h"

namespace oneui::views {
std::string Runtime::hostKey(const ScopePath &path, const std::string &name) {
    if (path.empty())
        return name; // WP3 root host keys stay byte-identical.
    std::string result(1, '\0');
    for (const auto &[id, key] : path) {
        result += std::to_string(id.size()) + ':' + id;
        result += std::to_string(key.size()) + ':' + key;
    }
    return result + ':' + name;
}
std::shared_ptr<Scope> Runtime::renderScope(const std::shared_ptr<Scope> &source,
                                            const std::shared_ptr<Instance> &instance,
                                            const std::string &key) {
    auto path = instance->scopes.front()->path;
    path.emplace_back(instance->repeatId, key);
    // Reload candidates borrow matching render connections, just as WP3 borrows
    // data scopes. Key reuse with a different VM must never revive an old handle.
    auto found = renderScopes.find(path);
    if (found != renderScopes.end() && !found->second->closed && found->second->vm == source->vm &&
        found->second->identity == source->identity)
        return found->second;
    auto result = acquire({source->identity, source->vm}, source->table.model_id, false);
    result->renderScope = true;
    result->sourceScope = source;
    result->path = std::move(path);
    connect(result);
    return result;
}
void Runtime::prepareWorkspaceIndex() {
    if (!image.count(oneview::WSC1))
        return;
    decltype(renderScopes) nextScopes;
    decltype(hosts) nextHosts;
    std::function<void(const std::shared_ptr<Instance> &)> walk = [&](const auto &n) {
        for (const auto &scope : n->scopes)
            if (scope->renderScope) {
                auto [at, added] = nextScopes.emplace(scope->path, scope);
                check(added || at->second == scope, ONEUI_VIEW_BAD_KEY,
                      "Duplicate workspace scope path");
            }
        if (n->element.component == "NativeHost") {
            const auto &record =
                image.records[oneview::HOST][image.records[oneview::NODE][n->node][9]];
            const auto key = hostKey(n->scopes.front()->path, image.strings[record[1]]);
            check(nextHosts.emplace(key, n->element.widget).second, ONEUI_VIEW_BAD_KEY,
                  "Duplicate scoped host");
        }
        for (const auto &child : n->children)
            walk(child);
        for (const auto &[key, row] : n->pendingRows ? *n->pendingRows : n->rows)
            walk(row);
    };
    walk(root);
    auto commit = [this, nextScopes = std::move(nextScopes),
                   nextHosts = std::move(nextHosts)]() mutable {
        // Disconnect native content before releasing its VM/callback owner.
        if (!building)
            for (const auto &[key, widget] : hosts)
                if (!nextHosts.count(key) || nextHosts.at(key) != widget)
                    if (auto host = std::dynamic_pointer_cast<ui::NativeHostView>(widget))
                        host->detach();
        if (!building)
            for (const auto &[path, scope] : renderScopes)
                if (!nextScopes.count(path) || nextScopes.at(path) != scope)
                    closeScope(scope);
        hosts.swap(nextHosts);
        renderScopes.swap(nextScopes);
    };
    if (staging)
        commits.push_back(std::move(commit));
    else
        commit();
}
} // namespace oneui::views
