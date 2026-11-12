#include "runtime.h"
#include "oneui/controls/time_series_chart.h"
#include <cmath>

namespace oneui::views {
using namespace oneview;

// Range endpoints are data, not nonnegative layout dimensions. Read both
// bound endpoints from the same current model before updating the widget.
// This also avoids narrowing f64 values through Mount's float layout setter.
bool Runtime::chartRangeScalar(Instance& instance, const std::string& name,
                               const Value& value) {
    if (instance.element.component != "TimeSeriesChart" ||
        (name != "range-min" && name != "range-max"))
        return false;

    auto chart = instance.element.as<TimeSeriesChart>();
    double minimum = name == "range-min" ? value.number : chart->minimum();
    double maximum = name == "range-max" ? value.number : chart->maximum();
    for (const auto& binding : instance.bindings) {
        if (binding.kind != BIND)
            continue;
        const auto& record = binding.record;
        const auto component = componentRoot(image, record[0]);
        const auto& propertyName = image.components[image.records[NODE][component][1]]
                                       .property(record[1]).name;
        if (propertyName != "range-min" && propertyName != "range-max")
            continue;
        auto scope = binding.scope.lock();
        if (!scope || scope->closed)
            continue;
        const auto endpoint = read(scope, record[2]);
        check(endpoint.tag == 3, ONEUI_VIEW_TYPE, "Chart range requires f64 endpoints");
        if (propertyName == "range-min")
            minimum = endpoint.number;
        else
            maximum = endpoint.number;
    }
    check(std::isfinite(minimum) && std::isfinite(maximum) && maximum > minimum,
          ONEUI_VIEW_TYPE, "Chart range requires finite minimum < maximum");
    chart->setRange(minimum, maximum);
    return true;
}
} // namespace oneui::views
