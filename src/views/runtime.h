#pragma once
#include "format/ir.h"
#include "oneui/oneui_view_c_api.h"
#include "oneui/ui_declarative.h"
#include <atomic>
#include <thread>
#include <unordered_map>

namespace oneui::views {
struct Runtime;
struct Scope;
struct Instance;
struct Failure : std::runtime_error {
    int result;
    Failure(int result, const char *message) : std::runtime_error(message), result(result) {}
};
inline void check(bool condition, int code, const char *message) {
    if (!condition)
        throw Failure(code, message);
}
struct Value {
    uint32_t tag = 0;
    int64_t integer = 0;
    double number = 0;
    std::string text;
    static Value copy(const OneUiValue &value);
    OneUiValue borrow() const;
    bool operator==(const Value &rhs) const;
};
struct Delta {
    uint32_t kind = 0;
    uint64_t before = 0, after = 0, index = 0, count = 0, to = 0;
    std::vector<std::string> keys;
    std::vector<uint32_t> properties;
};
struct ListState {
    uint64_t revision = 0;
    uint64_t serial = 0;
    bool initialized = false, reset = true;
    bool lastReset = true;
    std::vector<std::string> keys;
    std::vector<std::shared_ptr<Scope>> items;
    std::vector<Delta> pending;
    std::vector<Delta> applied;
    size_t queuedBytes = 0;
};
struct Scope {
    OneUiViewModelVTable table{};
    const OneUiViewModelVTable *identity = nullptr;
    void *vm = nullptr;
    OneUiViewNotifier *notifier = nullptr;
    bool retained = false, connected = false, closed = false, mutating = false;
    std::set<uint32_t> dirty;
    std::map<uint32_t, Value> values;
    std::map<uint32_t, uint64_t> versions;
    std::map<uint32_t, ListState> lists;
    std::map<uint32_t, std::vector<std::weak_ptr<Instance>>> observers;
    // Render connections have instance identity independent of list-data leases.
    bool renderScope = false;
    std::vector<std::pair<std::string, std::string>> path;
    std::weak_ptr<Scope> sourceScope;
};
using Scopes = std::vector<std::shared_ptr<Scope>>;
struct Binding {
    oneview::Record record;
    std::weak_ptr<Scope> scope;
    uint32_t kind;
    std::optional<Value> previous;
    // Static author classes plus generated scope tokens. Dynamic classes are
    // replaced against this base, never accumulated across notifications.
    std::optional<std::string> baseClasses;
};
struct ModelWrite {
    std::weak_ptr<Scope> scope;
    uint32_t property;
    Value expected, replacement;
};
struct SlotBody {
    uint32_t body;
    Scopes scopes;
};
using Slots = std::map<std::string, SlotBody>;
struct Instance {
    std::weak_ptr<Runtime> owner;
    uint32_t node = 0;
    ui::Element element{ui::Node(nullptr), ""};
    Scopes scopes;
    std::vector<std::shared_ptr<Instance>> children;
    std::vector<Binding> bindings;
    std::vector<oneview::Record> events;
    std::vector<uint32_t> owners;
    std::map<std::string, std::shared_ptr<Instance>> rows;
    std::vector<ui::Element> conditions;
    bool applying = false;
    bool authoredDisabled = false;
    bool published = false;
    bool dirty = true;
    std::weak_ptr<Instance> parent;
    std::map<std::string, std::vector<std::wstring>> stringLists;
    struct TableCache;
    std::shared_ptr<TableCache> tableCache;
    struct RichTableCache;
    std::shared_ptr<RichTableCache> richTableCache;
    struct PickerCache;
    std::shared_ptr<PickerCache> pickerCache;
    struct WorkspaceCache;
    std::shared_ptr<WorkspaceCache> workspaceCache;
    std::set<const Scope*> recordDirty;
    const ListState* repeatSource = nullptr;
    uint64_t repeatSerial = 0;
    bool transparent = false;
    std::string repeatId;
    std::shared_ptr<std::map<std::string, std::shared_ptr<Instance>>> pendingRows;
};
enum class Phase { Idle, Read, Mutate, Connect, Cleanup };
extern thread_local Runtime *callbackRuntime;
extern thread_local Phase callbackPhase;
struct Callback {
    Runtime &runtime;
    Runtime *previousRuntime;
    Phase previousPhase;
    Callback(Runtime &, Phase);
    ~Callback();
};
struct Runtime : std::enable_shared_from_this<Runtime> {
    oneview::Image image;
    std::array<uint8_t, 32> hash{};
    std::map<uint32_t, std::string> scopeClasses;
    std::thread::id thread = std::this_thread::get_id();
    std::shared_ptr<ui::StyleRegistry> styles = std::make_shared<ui::StyleRegistry>();
    ui::Mount factory;
    std::shared_ptr<Scope> rootScope;
    std::shared_ptr<Instance> root;
    std::vector<std::shared_ptr<Scope>> scopes;
    std::map<std::string, std::shared_ptr<Widget>> hosts;
    using ScopePath = std::vector<std::pair<std::string, std::string>>;
    std::map<ScopePath, std::shared_ptr<Scope>> renderScopes;
    std::string callPrefix;
    struct WorkspaceDomains;
    std::shared_ptr<WorkspaceDomains> workspaceDomains;
    std::function<void()> schedule, detach;
    std::function<void()> republish;
    std::string externalCss;
    ui::Density density = ui::Density::Comfortable;
    bool closing = false, closed = false, building = true, flushing = false, scheduled = false;
    unsigned depth = 0;
    bool staging = false;
    std::vector<std::function<void()>> commits, rollbacks;
    std::vector<std::shared_ptr<Scope>> retired;
    std::vector<ModelWrite> modelWrites;
    std::weak_ptr<Instance> observing;
    std::vector<std::weak_ptr<Runtime>> deferredSchedules;
    OneUiViewError error{};
    Runtime(oneview::Image image, std::shared_ptr<Clipboard> clipboard = {},
            std::shared_ptr<ui::StyleRegistry> sharedStyles = {});
    ~Runtime();
    void fail(int result, const char *message, uint32_t span = oneview::none);
    void alive() const;
    void onThread() const;
    void checkTable(const OneUiViewModelVTable *, uint32_t model) const;
    std::shared_ptr<Scope> acquire(const OneUiViewModelRef &, uint32_t model, bool transferred);
    void connect(const std::shared_ptr<Scope> &);
    void closeScope(const std::shared_ptr<Scope> &);
    void close();
    void finishClose();
    void initialize(const OneUiViewModelVTable *, void *, bool sharedStyles = false);
    void reload(oneview::Image);
    Value read(const std::shared_ptr<Scope> &, uint32_t property);
    Value constant(uint32_t index) const;
    void notify(const std::shared_ptr<Scope> &, uint32_t property);
    void notifyList(const std::shared_ptr<Scope> &, uint32_t, const OneUiViewListChange &);
    void enqueue();
    void observe(const std::shared_ptr<Scope> &, uint32_t);
    void mark(const std::shared_ptr<Scope> &, uint32_t);
    void flush();
    void flushPass();
    void update(const std::shared_ptr<Instance> &);
    void refreshRepeatLayouts(const std::shared_ptr<Instance>&);
    void apply(const std::shared_ptr<Instance> &, Binding &);
    void invoke(const std::shared_ptr<Instance> &, const oneview::Record &, std::vector<Value>);
    void set(const std::shared_ptr<Scope> &, uint32_t, const Value &);
    ListState &list(const std::shared_ptr<Scope> &, uint32_t);
    void refreshList(const std::shared_ptr<Scope> &, uint32_t, ListState &);
    bool refreshDelta(const std::shared_ptr<Scope> &, uint32_t, ListState &,
                      const OneUiViewListInfo &);
    void auditListIdentity(const std::shared_ptr<Scope> &, uint32_t, const ListState &,
                           const std::vector<std::string> &);
    std::shared_ptr<Instance> build(uint32_t node, const Scopes &, const Slots & = {});
    void install(const std::shared_ptr<Instance> &, uint32_t owner, const Scopes &);
    void wire(const std::shared_ptr<Instance> &);
    void wireRichTable(const std::shared_ptr<Instance>&);
    void applyRichTableRows(Instance&, ListState&);
    void repeat(const std::shared_ptr<Instance> &);
    std::shared_ptr<Instance> buildVirtual(uint32_t, const Scopes&);
    void virtualRepeat(const std::shared_ptr<Instance>&);
    void prepareWorkspaceIndex();
    void workspaceChildren(const std::shared_ptr<Instance> &);
    bool pickerScalar(Instance&, const std::string&, const Value&);
    bool pickerList(Instance&, const std::string&, ListState&);
    void preparePicker(const std::shared_ptr<Instance>&);
    void wirePicker(const std::shared_ptr<Instance>&);
    bool dockScalar(Instance&, const std::string&, const Value&);
    bool dockList(Instance&, const std::string&, ListState&);
    void prepareDock(const std::shared_ptr<Instance>&, const std::vector<std::shared_ptr<Widget>>&);
    void wireDock(const std::shared_ptr<Instance>&);
    bool workspaceScalar(Instance&, const std::string&, const Value&);
    bool paneHeaderScalar(Instance&, const std::string&, const Value&);
    bool dragScalar(Instance&, const std::string&, const Value&);
    bool dragList(Instance&, const std::string&, ListState&);
    void wireDrag(const std::shared_ptr<Instance>&);
    std::function<void()> prepareDragGrid(const std::shared_ptr<Instance>&);
    bool workspaceList(Instance&, const std::string&, ListState&);
    void prepareWorkspace(const std::shared_ptr<Instance>&, const std::vector<std::shared_ptr<Widget>>&);
    void wireWorkspace(const std::shared_ptr<Instance>&);
    std::shared_ptr<Scope> renderScope(const std::shared_ptr<Scope> &, const std::shared_ptr<Instance> &,
                                     const std::string &key);
    static std::string hostKey(const ScopePath &, const std::string &);
    void applyList(Instance &, const std::string &, ListState &);
    void applyTableRows(Instance &, ListState &);
};
std::string narrow(const std::wstring &);
struct Observation {
    Runtime &runtime;
    std::weak_ptr<Instance> previous;
    Observation(Runtime &runtime, const std::shared_ptr<Instance> &node)
        : runtime(runtime), previous(runtime.observing) {
        runtime.observing = node;
    }
    ~Observation() { runtime.observing = previous; }
};
void setScalar(ui::Mount &, ui::Element &, const std::string &, const Value &);
void emitComponentEvent(const std::weak_ptr<Instance>&, const std::string&, std::vector<Value>);
} // namespace oneui::views

// Notifier owns no VM reference. It may safely outlive its runtime on any thread.
struct OneUiViewNotifier {
    std::atomic<unsigned> references{1};
    std::weak_ptr<oneui::views::Runtime> runtime;
    std::weak_ptr<oneui::views::Scope> scope;
    std::thread::id thread;
};
