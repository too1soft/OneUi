#include "runtime.h"
#include <cstring>
namespace oneui::views {
thread_local Runtime *callbackRuntime = nullptr;
thread_local Phase callbackPhase = Phase::Idle;
Callback::Callback(Runtime &r, Phase p)
    : runtime(r), previousRuntime(callbackRuntime), previousPhase(callbackPhase) {
    ++r.depth;
    callbackRuntime = &r;
    callbackPhase = p;
}
Callback::~Callback() {
    callbackRuntime = previousRuntime;
    callbackPhase = previousPhase;
    if (--runtime.depth == 0 && runtime.closing && !runtime.building && !runtime.flushing)
        runtime.finishClose();
    if (!runtime.depth && previousRuntime == nullptr) {
        auto pending = std::move(runtime.deferredSchedules);
        for (auto &weak : pending)
            if (auto view = weak.lock()) {
                try {
                    view->enqueue();
                } catch (...) {
                    view->fail(ONEUI_VIEW_OUT_OF_MEMORY, "Deferred scheduling failed");
                }
            }
    }
}
Runtime::Runtime(oneview::Image im, std::shared_ptr<Clipboard> clipboard,
                 std::shared_ptr<ui::StyleRegistry> sharedStyles)
    : image(std::move(im)), hash(oneview::sha256(image.schema.encode())),
      styles(sharedStyles ? std::move(sharedStyles) : std::make_shared<ui::StyleRegistry>()),
      factory({}, styles, std::move(clipboard)) {
    error.struct_size = sizeof(error);
    error.record_index = oneview::none;
    error.source_map_index = oneview::none;
    for (const auto &record : image.records[oneview::NODE]) {
        if(record[10]==oneview::none || scopeClasses.count(record[8]))continue;
        const auto &file = image.strings[image.records[oneview::SMAP][record[10]][0]];
        oneview::Bytes bytes(file.begin(), file.end());
        bytes.push_back(0);
        auto stem = std::filesystem::path(file).stem().generic_string();
        bytes.insert(bytes.end(), stem.begin(), stem.end());
        scopeClasses[record[8]] = "one-s-" + oneview::hex(oneview::sha256(bytes)).substr(0, 16);
    }
}
Runtime::~Runtime() { close(); }
void Runtime::onThread() const {
    check(thread == std::this_thread::get_id(), ONEUI_VIEW_WRONG_THREAD, "View requires UI thread");
}
void Runtime::alive() const { check(!closing && !closed, ONEUI_VIEW_CLOSED, "View is closed"); }
void Runtime::fail(int result, const char *message, uint32_t span) {
    error.result = result;
    ++error.sequence;
    error.source_map_index = span;
    std::strncpy(error.message, message, sizeof(error.message) - 1);
    error.message[sizeof(error.message) - 1] = 0;
}
void Runtime::checkTable(const OneUiViewModelVTable *t, uint32_t model) const {
    check(t && t->struct_size >= sizeof(*t) && t->protocol_version == 1, ONEUI_VIEW_VERSION,
          "VM protocol size/version");
    check(t->struct_size <= 4096, ONEUI_VIEW_VERSION, "VM table extension too large");
    const auto *tail = reinterpret_cast<const uint8_t *>(t) + sizeof(*t);
    check(std::all_of(tail, reinterpret_cast<const uint8_t *>(t) + t->struct_size,
                      [](uint8_t x) { return !x; }),
          ONEUI_VIEW_VERSION, "VM table unknown extension");
    check(t->flags == 0 && t->model_id == model &&
              std::equal(hash.begin(), hash.end(), t->schema_hash),
          ONEUI_VIEW_SCHEMA_MISMATCH, "VM schema/model mismatch");
    check(t->retain && t->release && t->connect && t->disconnect && t->get,
          ONEUI_VIEW_INVALID_ARGUMENT, "Missing mandatory VM callback");
    const auto &m = image.schema.model(model);
    for (const auto &entry : m.properties) {
        const auto &p = entry.second;
        if (p.access == 3)
            check(t->set, ONEUI_VIEW_INVALID_ARGUMENT, "Missing set callback");
        if (p.tag == 5)
            check(t->list_info && t->list_key && t->list_acquire, ONEUI_VIEW_INVALID_ARGUMENT,
                  "Missing list callbacks");
    }
    check(m.commands.empty() || t->invoke, ONEUI_VIEW_INVALID_ARGUMENT, "Missing invoke callback");
}
std::shared_ptr<Scope> Runtime::acquire(const OneUiViewModelRef &ref, uint32_t model,
                                        bool transferred) {
    check(ref.vm, ONEUI_VIEW_INVALID_ARGUMENT, "Null VM");
    checkTable(ref.vtable, model);
    auto scope = std::make_shared<Scope>();
    scope->table = *ref.vtable;
    scope->identity = ref.vtable;
    scope->vm = ref.vm;
    scopes.push_back(scope);
    if (!transferred) {
        Callback guard(*this, Phase::Cleanup);
        scope->table.retain(scope->vm);
    }
    scope->retained = true;
    alive();
    return scope;
}
void Runtime::connect(const std::shared_ptr<Scope> &scope) {
    auto *notifier = new OneUiViewNotifier;
    notifier->runtime = shared_from_this();
    notifier->scope = scope;
    notifier->thread = thread;
    scope->notifier = notifier;
    scope->connected = true;
    int result;
    {
        Callback guard(*this, Phase::Connect);
        result = scope->table.connect(scope->vm, notifier);
    }
    alive();
    check(result == 0, result < 0 ? result : ONEUI_VIEW_FOREIGN_FAILURE, "VM connect failed");
}
void Runtime::closeScope(const std::shared_ptr<Scope> &scope) {
    if (!scope || scope->closed)
        return;
    scope->closed = true;
    for (auto &entry : scope->lists)
        for (auto &item : entry.second.items)
            closeScope(item);
    scope->lists.clear();
    if (scope->connected) {
        scope->connected = false;
        try {
            Callback guard(*this, Phase::Cleanup);
            scope->table.disconnect(scope->vm, scope->notifier);
        } catch (...) {
        }
    }
    scope->values.clear();
    scope->dirty.clear();
    if (scope->retained) {
        scope->retained = false;
        try {
            Callback guard(*this, Phase::Cleanup);
            scope->table.release(scope->vm);
        } catch (...) {
        }
    }
    scope->vm = nullptr;
    if (scope->notifier && --scope->notifier->references == 0)
        delete scope->notifier;
    scope->notifier = nullptr;
}
void Runtime::close() {
    if (closed)
        return;
    closing = true;
    scheduled = false;
    if (detach) {
        auto fn = std::move(detach);
        fn();
    }
    if (!depth && !building && !flushing)
        finishClose();
}
void Runtime::finishClose() {
    if (closed || depth)
        return;
    closed = true; // Prevent recursive cleanup from guard epilogues.
    if (root && root->element.widget) {
        auto w = root->element.widget;
        w->clearInteractionState();
        w->setInvalidator({});
        w->setRectInvalidator({});
        w->setAnimationScheduler({});
    }
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
        closeScope(*it);
    for (auto &entry : hosts)
        if (auto host = std::dynamic_pointer_cast<ui::NativeHostView>(entry.second))
            host->detach();
    root.reset();
    rootScope.reset();
    scopes.clear();
    hosts.clear();
}
Value Runtime::read(const std::shared_ptr<Scope> &scope, uint32_t id) {
    observe(scope, id);
    alive();
    check(!scope->closed, ONEUI_VIEW_CLOSED, "Scope is closed");
    auto found = scope->values.find(id);
    if (found != scope->values.end() && !scope->dirty.count(id))
        return found->second;
    auto &property = image.schema.model(scope->table.model_id).property(id);
    check(property.tag != 5, ONEUI_VIEW_TYPE, "List is not scalar");
    OneUiValue borrowed{};
    Value owned;
    int result;
    {
        Callback guard(*this, Phase::Read);
        result = scope->table.get(scope->vm, id, &borrowed);
        if (result == 0)
            owned = Value::copy(borrowed); // Before guard epilogue can release VM.
    }
    alive();
    check(result == 0, result < 0 ? result : ONEUI_VIEW_FOREIGN_FAILURE, "VM get failed");
    check(owned.tag == property.tag, ONEUI_VIEW_TYPE, "VM property type mismatch");
    scope->values[id] = owned;
    scope->dirty.erase(id);
    return owned;
}
} // namespace oneui::views
