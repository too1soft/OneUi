#pragma once
#include "oneui/view_cpp_adapter.h"

namespace oneui::ui::view {
// C++ authoring's value collections are normalized at the adapter boundary.
// Builtins without authored IDs have positional keys; keyed business lists use
// shared_ptr items and never take this path.
struct StringRecord { std::string id; std::wstring value; };
struct NumberRecord { std::string id; double value; };
struct ColumnRecord {
    std::string id;
    std::wstring header;
    double width;
    int64_t alignment, kind;
    bool action;
};
struct RowRecord { std::string id; std::vector<std::wstring> cells; };
struct TabRecord { std::string id; std::wstring title; };
struct SeriesRecord {
    std::string id;
    std::wstring name;
    int64_t rgba;
    std::vector<double> values;
};
inline StringRecord stringRecord(const std::wstring &v,size_t i) { return {std::to_string(i),v}; }
inline NumberRecord numberRecord(double v,size_t i) { return {std::to_string(i),v}; }
inline ColumnRecord columnRecord(const TableColumn &v,size_t i) {
    return {std::to_string(i),v.header,v.width,int64_t(v.alignment),int64_t(v.kind),v.action};
}
template<class T> RowRecord rowRecord(const T &v,size_t) { return {utf8(v.id),v.cells}; }
template<class T> TabRecord tabRecord(const T &v,size_t) { return {utf8(v.id),v.title}; }
inline SeriesRecord seriesRecord(const TimeSeriesChartSeries &v,size_t i) {
    const auto c=v.color;
    return {std::to_string(i),v.name,int64_t((uint32_t(c.r)<<24)|(uint32_t(c.g)<<16)|(uint32_t(c.b)<<8)|c.a),v.values};
}
template<class Source,class Convert,class Build>
void recordList(Adapter &a,uint32_t id,Source &source,Convert convert,Build build) {
    using Record=decltype(convert(field(source).front(),size_t{}));
    struct Row { std::shared_ptr<Record> value; std::weak_ptr<Adapter> adapter; };
    struct Snapshot { std::vector<std::string> keys; std::map<std::string,Row> rows; };
    auto snapshot=std::make_shared<Snapshot>();
    List list;
    list.size=[snapshot,&source,convert] {
        std::vector<std::string> keys;
        std::map<std::string,Row> next;
        const auto &values=field(source);
        for(size_t i=0;i<values.size();++i) {
            auto value=convert(values[i],i);
            require(!value.id.empty() && !next.count(value.id));
            keys.push_back(value.id);
            auto found=snapshot->rows.find(value.id);
            if(found!=snapshot->rows.end()) {
                *found->second.value=std::move(value);
                next.emplace(keys.back(),found->second);
            } else next.emplace(keys.back(),Row{std::make_shared<Record>(std::move(value)),{}});
        }
        snapshot->keys.swap(keys);snapshot->rows.swap(next);
        return snapshot->keys.size();
    };
    list.key=[snapshot](size_t i) { return snapshot->keys.at(i); };
    list.item=[snapshot,build](size_t i) {
        auto &row=snapshot->rows.at(snapshot->keys.at(i));
        if(auto a=row.adapter.lock())return a;
        auto a=build(row.value);row.adapter=a;return a;
    };
    a.lists.emplace(id,std::move(list));
}
}
