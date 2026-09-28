#pragma once
#include "oneui/ui_declarative.h"

namespace oneui::ui {
namespace compose_detail {
template<schema::ValueType> struct Value;
template<> struct Value<schema::ValueType::Text> { using type=std::wstring; };
template<> struct Value<schema::ValueType::Boolean> { using type=bool; };
template<> struct Value<schema::ValueType::Integer> { using type=int; };
template<> struct Value<schema::ValueType::Number> { using type=float; };
template<> struct Value<schema::ValueType::Strings> { using type=std::vector<std::wstring>; };
template<> struct Value<schema::ValueType::Columns> { using type=std::vector<TableColumn>; };
template<> struct Value<schema::ValueType::Rows> { using type=std::vector<TableRow>; };
template<class T,class=void> struct Observable:std::false_type {};
template<class T> struct Observable<T,std::void_t<decltype(std::declval<T&>().get())>>:std::true_type {};

template<schema::ValueType Type,class V>
void assign(Mount& mount,Element& element,const char* key,V&& value) {
    using Expected=typename Value<Type>::type;
    if constexpr(Observable<std::remove_reference_t<V>>::value) {
        static_assert(std::is_lvalue_reference_v<V>,"Compose binding requires a persistent State/Computed lvalue");
        constexpr bool matches=std::is_same_v<Expected,std::decay_t<decltype(value.get())>>;
        static_assert(matches,"Compose property and State/Computed value types do not match");
        if constexpr(matches && std::is_lvalue_reference_v<V>)mount.bindTyped<Expected>(element,key,value);
    } else {
        constexpr bool matches=std::is_same_v<Expected,std::decay_t<V>> || (Type==schema::ValueType::Text && std::is_convertible_v<V,Expected>);
        static_assert(matches,"Compose property value has the wrong type");
        if constexpr(matches)mount.set(element,key,Expected(std::forward<V>(value)));
    }
}
} // namespace compose_detail

// A short-lived authoring handle. It owns no subscriptions or extra native widgets.
// Mount and bound ViewModel values must outlive handle use, as with Mount itself.
template<schema::Kind Kind> class Part {
    Mount* mount_;
    Element element_;
public:
    Part(Mount& mount,Element element):mount_(&mount),element_(std::move(element)) {
        if(element_.component!=schema::component(Kind).name)throw std::invalid_argument("Compose component kind mismatch");
    }
    operator Element() const { return element_; }
    const Element& element() const { return element_; }
    std::shared_ptr<Widget> widget() const { return element_.widget; }
    Part& ref(const std::string& name) { mount_->remember(name,element_.widget);return *this; }
    Part& located(const char* file,int line) { mount_->locate(element_,file,line);return *this; }
    Part& classes(const std::string& value) { mount_->set(element_,"class",value);return *this; }
#define ONEUI_COMPOSE_PROPERTY(method,key) \
    template<class V> Part& method(V&& value) { \
        static_assert(schema::supports(Kind,key),"Compose property is not supported by this component"); \
        if constexpr(schema::supports(Kind,key)) \
            compose_detail::assign<schema::propertyType(Kind,key)>(*mount_,element_,key,std::forward<V>(value)); \
        return *this; \
    }
    ONEUI_COMPOSE_PROPERTY(open,"open")
    ONEUI_COMPOSE_PROPERTY(preset,"preset")
    ONEUI_COMPOSE_PROPERTY(reducedMotion,"reduced-motion")
    ONEUI_COMPOSE_PROPERTY(text,"text")
    ONEUI_COMPOSE_PROPERTY(title,"title")
    ONEUI_COMPOSE_PROPERTY(subtitle,"subtitle")
    ONEUI_COMPOSE_PROPERTY(label,"label")
    ONEUI_COMPOSE_PROPERTY(hint,"hint")
    ONEUI_COMPOSE_PROPERTY(error,"error")
    ONEUI_COMPOSE_PROPERTY(placeholder,"placeholder")
    ONEUI_COMPOSE_PROPERTY(name,"name")
    ONEUI_COMPOSE_PROPERTY(description,"description")
    ONEUI_COMPOSE_PROPERTY(tone,"tone")
    ONEUI_COMPOSE_PROPERTY(items,"items")
    ONEUI_COMPOSE_PROPERTY(columns,"columns")
    ONEUI_COMPOSE_PROPERTY(disabled,"disabled")
    ONEUI_COMPOSE_PROPERTY(visible,"visible")
#undef ONEUI_COMPOSE_PROPERTY
    template<class T> Part& model(State<T>& state) {
        static_assert(schema::component(Kind).model!=schema::ValueType::None,"Compose model is not supported by this component");
        if constexpr(schema::component(Kind).model!=schema::ValueType::None) {
            using Expected=typename compose_detail::Value<schema::component(Kind).model>::type;
            static_assert(std::is_same_v<T,Expected>,"Compose model requires the component's exact State type");
            if constexpr(std::is_same_v<T,Expected>)mount_->model(element_,state);
        }
        return *this;
    }
    Part& onClick(VmCommand& command) {
        static_assert(Kind==schema::Kind::Button,"Compose onClick requires Button");mount_->click(element_,command);return *this;
    }
    Part& onActivate(VmCommand& command) {
        static_assert(Kind==schema::Kind::DataTable,"Compose onActivate requires DataTable");mount_->tableEvent(element_,"activate",command);return *this;
    }
    Part& onDelete(VmCommand& command) {
        static_assert(Kind==schema::Kind::DataTable,"Compose onDelete requires DataTable");mount_->tableEvent(element_,"delete",command);return *this;
    }
    Part& primary() { static_assert(Kind==schema::Kind::Button,"Compose primary requires Button");mount_->set(element_,"variant",std::string("primary"));return *this; }
    Part& danger() { static_assert(Kind==schema::Kind::Button,"Compose danger requires Button");mount_->set(element_,"variant",std::string("danger"));return *this; }
    Part& grow(float value=1) {mount_->set(element_,"grow",value);return *this;}
    Part& basis(float value) {mount_->set(element_,"basis",value);return *this;}
    Part& shrink(float value) {mount_->set(element_,"shrink",value);return *this;}
    Part& minColumnWidth(float value) {
        static_assert(Kind==schema::Kind::FormGrid,"Compose minColumnWidth requires FormGrid");mount_->set(element_,"min-column-width",value);return *this;
    }
};

class Compose {
    Mount& mount_;
    template<schema::Kind Kind> Part<Kind> make(std::vector<Element> children={}) {
        return {mount_,mount_.make(schema::component(Kind).name,std::move(children))};
    }
public:
    explicit Compose(Mount& mount):mount_(mount) {}
#define ONEUI_COMPOSE_CONTAINER(method,kind) \
    auto method(std::vector<Element> children={}) {return make<schema::Kind::kind>(std::move(children));}
    ONEUI_COMPOSE_CONTAINER(page,Page)
    ONEUI_COMPOSE_CONTAINER(header,Header)
    ONEUI_COMPOSE_CONTAINER(toolbar,Toolbar)
    ONEUI_COMPOSE_CONTAINER(section,Section)
    ONEUI_COMPOSE_CONTAINER(settingsPage,SettingsPage)
    ONEUI_COMPOSE_CONTAINER(listPage,ListPage)
    ONEUI_COMPOSE_CONTAINER(detailPage,DetailPage)
    ONEUI_COMPOSE_CONTAINER(actions,ActionBar)
    ONEUI_COMPOSE_CONTAINER(column,Column)
    ONEUI_COMPOSE_CONTAINER(row,Row)
    ONEUI_COMPOSE_CONTAINER(content,Content)
    ONEUI_COMPOSE_CONTAINER(emptyState,EmptyState)
    ONEUI_COMPOSE_CONTAINER(loadingState,LoadingState)
#undef ONEUI_COMPOSE_CONTAINER
    auto reveal(Element child) {return make<schema::Kind::Reveal>({std::move(child)});}
    auto scroll(Element child) {return make<schema::Kind::Scroll>({std::move(child)});}
    auto field(const std::wstring& label,Element control) {return make<schema::Kind::FormRow>({std::move(control)}).label(label);}
    auto formGrid(std::initializer_list<Part<schema::Kind::FormRow>> fields) {
        std::vector<Element> children;children.reserve(fields.size());
        for(const auto& field:fields)children.push_back(field.element());
        return make<schema::Kind::FormGrid>(std::move(children));
    }
    template<class V> auto text(V&& value) {return make<schema::Kind::Text>().text(std::forward<V>(value));}
    template<class V> auto status(V&& value) {return make<schema::Kind::Status>().text(std::forward<V>(value));}
    template<class V> auto validation(V&& value) {return make<schema::Kind::ValidationMessage>().text(std::forward<V>(value));}
    template<class V> auto button(V&& value,VmCommand& command) {return make<schema::Kind::Button>().text(std::forward<V>(value)).onClick(command);}
    auto input(State<std::wstring>& state) {return make<schema::Kind::Input>().model(state);}
    auto search(State<std::wstring>& state) {return make<schema::Kind::SearchInput>().model(state);}
    auto toggle(State<bool>& state) {return make<schema::Kind::Switch>().model(state);}
    template<class Items> auto select(Items&& items,State<int>& state) {return make<schema::Kind::Select>().items(std::forward<Items>(items)).model(state);}
    template<class Columns,class Rows> auto table(Columns&& columns,Rows&& rows,State<std::wstring>& selection) {
        return make<schema::Kind::DataTable>().columns(std::forward<Columns>(columns)).items(std::forward<Rows>(rows)).model(selection);
    }
};
} // namespace oneui::ui
