#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace oneui::views {

struct Scope;

// All nodes in a lexical scope share its ordered binding context. Entering
// a repeat row prepends a scope without changing the containing context.
class Scopes {
    using Entries = std::vector<std::shared_ptr<Scope>>;
    std::shared_ptr<const Entries> scopes_;

    explicit Scopes(std::shared_ptr<const Entries> scopes) : scopes_(std::move(scopes)) {}

    const Entries& entries() const {
        static const Entries empty;
        return scopes_ ? *scopes_ : empty;
    }

public:
    Scopes() = default;
    explicit Scopes(const std::shared_ptr<Scope>& scope)
        : scopes_(std::make_shared<const Entries>(1, scope)) {}

    Scopes withRow(const std::shared_ptr<Scope>& scope) const {
        const auto& parent = entries();
        auto child = std::make_shared<Entries>();
        child->reserve(parent.size() + 1);
        child->push_back(scope);
        child->insert(child->end(), parent.begin(), parent.end());
        return Scopes(std::move(child));
    }

    const std::shared_ptr<Scope>& at(std::size_t index) const { return entries().at(index); }
    const std::shared_ptr<Scope>& operator[](std::size_t index) const { return entries()[index]; }
    const std::shared_ptr<Scope>& front() const { return entries().front(); }
    std::size_t size() const { return entries().size(); }
    Entries::const_iterator begin() const { return entries().begin(); }
    Entries::const_iterator end() const { return entries().end(); }
};

} // namespace oneui::views
