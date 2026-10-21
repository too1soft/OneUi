#pragma once
// Generated C++ adapters share the frozen foreign-VM protocol with Rust.
#include "oneui/oneui_view_c_api.h"
#include "oneui/ui_declarative.h"
#include <limits>

namespace oneui::ui::view {
inline std::string utf8(const std::wstring &s) {
    return unicode::toUtf8(s);
}
inline std::string utf8(const std::string &s) { return s; }
inline std::wstring wide(const std::string &s) {
    return unicode::fromUtf8(s);
}
template<class T> auto fieldImpl(T &v, int) -> decltype(v.get()) { return v.get(); }
template<class T> T &fieldImpl(T &v, long) { return v; }
template<class T> decltype(auto) field(T &v) { return fieldImpl(v, 0); }
struct Value {
    OneUiValue pod{};
    std::string text;
    template<class T> static Value boolean(T v) {
        static_assert(std::is_same_v<T,bool>,"Schema bool requires C++ bool");
        Value r;r.pod.tag=1;r.pod.as.boolean=v;return r;
    }
    template<class T> static Value integer(T v) {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T,bool>,"Schema i64 requires C++ integer");
        Value r;r.pod.tag=2;r.pod.as.integer=v;return r;
    }
    template<class T> static Value number(T v) {
        static_assert(std::is_floating_point_v<T>,"Schema f64 requires C++ float/double");
        Value r;r.pod.tag=3;r.pod.as.number=v;return r;
    }
    template<class T> static Value string(const T &v) { Value r; r.pod.tag=4; r.text=utf8(v); return r; }
    OneUiValue borrow() const {
        auto result=pod;
        if(result.tag==4) result.as.utf8={text.data(),text.size()};
        return result;
    }
};
inline void require(bool ok) { if(!ok) throw std::invalid_argument("View adapter value type/range"); }
inline std::wstring text(const OneUiValue &v) {
    require(v.tag==4 && (!v.as.utf8.length || v.as.utf8.data));
    return wide(std::string(v.as.utf8.data ? v.as.utf8.data : "",v.as.utf8.length));
}
template<class S> void assign(S &source,const OneUiValue &v) {
    using T=std::decay_t<decltype(source.get())>;
    if constexpr(std::is_same_v<T,std::wstring>) source.set(text(v));
    else if constexpr(std::is_same_v<T,std::string>) source.set(utf8(text(v)));
    else if constexpr(std::is_same_v<T,bool>) { require(v.tag==1 && v.as.boolean<=1);source.set(v.as.boolean!=0); }
    else if constexpr(std::is_integral_v<T>) {
        require(v.tag==2 && v.as.integer>=std::numeric_limits<T>::lowest() && v.as.integer<=std::numeric_limits<T>::max());
        source.set(static_cast<T>(v.as.integer));
    } else { require(v.tag==3); source.set(static_cast<T>(v.as.number)); }
}
struct Adapter;
struct List {
    uint64_t revision=1;
    std::function<size_t()> size;
    std::function<std::string(size_t)> key;
    std::function<std::shared_ptr<Adapter>(size_t)> item;
};
struct Connection {
    OneUiViewNotifier *notifier;
    std::vector<Subscription> subscriptions;
    explicit Connection(OneUiViewNotifier *n):notifier(n) { oneui_view_notifier_retain(n); }
    ~Connection() { subscriptions.clear();oneui_view_notifier_release(notifier); }
};
struct Adapter : std::enable_shared_from_this<Adapter> {
    OneUiViewModelVTable table{};
    std::shared_ptr<void> owner;
    std::function<Value(uint32_t)> get;
    std::function<int(uint32_t,const OneUiValue &)> set;
    std::function<int(uint32_t,const OneUiValue *,size_t)> invoke;
    std::function<void(Adapter &,Connection &)> subscribe;
    std::map<uint32_t,List> lists;
    std::map<OneUiViewNotifier *,std::unique_ptr<Connection>> connections;
    std::map<std::pair<uint32_t,const void *>,std::weak_ptr<Adapter>> children;
    Value scratch;
    // Native leases keep this control alive; application ownership stays independent.
    unsigned leases=0;
    std::shared_ptr<Adapter> nativeLease;
    void retain() { if(!leases++) nativeLease=shared_from_this(); }
    void release() { if(!--leases) { auto last=std::move(nativeLease); } }
    template<class F> static int guard(F &&f) noexcept {
        try { return f(); }
        catch(const std::invalid_argument &) { return ONEUI_VIEW_TYPE; }
        catch(const std::bad_alloc &) { return ONEUI_VIEW_OUT_OF_MEMORY; }
        catch(...) { return ONEUI_VIEW_FOREIGN_FAILURE; }
    }
    Adapter(const uint8_t *hash,uint32_t model) {
        table.struct_size=sizeof(table);table.protocol_version=ONEUI_VIEW_PROTOCOL_VERSION;
        std::copy(hash,hash+32,table.schema_hash);table.model_id=model;
        table.retain=[](void *p) { guard([&] { static_cast<Adapter *>(p)->retain();return 0; }); };
        table.release=[](void *p) { guard([&] { static_cast<Adapter *>(p)->release();return 0; }); };
        table.connect=[](void *p,OneUiViewNotifier *n) {
            return guard([&] {
                auto &a=*static_cast<Adapter *>(p);
                auto c=std::make_unique<Connection>(n);
                auto *connection=c.get();a.connections.emplace(n,std::move(c));
                if(a.subscribe) a.subscribe(a,*connection);
                return 0;
            });
        };
        table.disconnect=[](void *p,OneUiViewNotifier *n) {
            guard([&] { static_cast<Adapter *>(p)->connections.erase(n);return 0; });
        };
        table.get=[](void *p,uint32_t id,OneUiValue *out) {
            *out={};return guard([&] {
                auto &a=*static_cast<Adapter *>(p);a.scratch=a.get(id);*out=a.scratch.borrow();return 0;
            });
        };
        table.set=[](void *p,uint32_t id,const OneUiValue *v) {
            return guard([&] { return static_cast<Adapter *>(p)->set(id,*v); });
        };
        table.invoke=[](void *p,uint32_t id,const OneUiValue *v,size_t count) {
            return guard([&] { return static_cast<Adapter *>(p)->invoke(id,v,count); });
        };
        table.list_info=[](void *p,uint32_t id,OneUiViewListInfo *out) {
            *out={};return guard([&] {
                auto &a=*static_cast<Adapter *>(p);auto &l=a.lists.at(id);
                for(auto it=a.children.begin();it!=a.children.end();)
                    if(it->second.expired()) it=a.children.erase(it); else ++it;
                out->length=l.size();out->revision=l.revision;return 0;
            });
        };
        table.list_key=[](void *p,uint32_t id,uint64_t index,OneUiValue *out) {
            *out={};return guard([&] {
                auto &a=*static_cast<Adapter *>(p);a.scratch=Value::string(a.lists.at(id).key(size_t(index)));
                *out=a.scratch.borrow();return 0;
            });
        };
        table.list_acquire=[](void *p,uint32_t id,uint64_t index,OneUiViewModelRef *out) {
            *out={};return guard([&] {
                auto child=static_cast<Adapter *>(p)->lists.at(id).item(size_t(index));
                child->retain();out->vtable=&child->table;out->vm=child.get();return 0;
            });
        };
    }
};
template<class S> auto watchImpl(S &s,Adapter &a,Connection &c,uint32_t id,int)
    -> decltype(s.subscribeScoped(std::function<void(const std::decay_t<decltype(s.get())>&)>{}),void()) {
    auto weak=a.weak_from_this();auto *n=c.notifier;
    c.subscriptions.push_back(s.subscribeScoped([weak,n,id](const auto &) {
        if(auto adapter=weak.lock()) {
            if(adapter->lists.count(id)) ++adapter->lists.at(id).revision;
            oneui_view_notifier_notify(n,id);
        }
    }));
}
template<class S> void watchImpl(S &,Adapter &,Connection &,uint32_t,long) {}
template<class S> void watch(S &s,Adapter &a,Connection &c,uint32_t id) { watchImpl(s,a,c,id,0); }
template<class S,class Key,class Build> void list(Adapter &adapter,uint32_t id,S &source,Key key,Build build) {
    auto weak=adapter.weak_from_this();
    List l;
    l.size=[&source] { return field(source).size(); };
    l.key=[&source,key](size_t index) { return utf8(key(field(source).at(index))); };
    l.item=[weak,&source,build,id](size_t index) {
        auto parent=weak.lock();require(bool(parent));
        auto item=field(source).at(index);require(bool(item));
        auto &slot=parent->children[{id,item.get()}];
        if(auto existing=slot.lock()) return existing;
        auto child=build(item);slot=child;return child;
    };
    adapter.lists.emplace(id,std::move(l));
}
// Implemented in oneui_views; no Window or platform dependency.
ONEUI_API Element mount(Mount &,const uint8_t *,size_t,const OneUiViewModelVTable *,void *);
} // namespace oneui::ui::view
