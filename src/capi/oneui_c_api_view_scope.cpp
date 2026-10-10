#include "oneui_c_api_view_internal.h"

using namespace oneui::capi::views;
OneUiViewResult oneui_view_resolve_scope(OneUiView *view, const OneUiViewScopeSegment *segments,
                                         size_t count, OneUiViewNotifier **out) {
    if (out)
        *out = nullptr;
    return call(__func__, [&] {
        check(out && (!count || segments), ONEUI_VIEW_INVALID_ARGUMENT,
              "Scope path/output required");
        auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Resolve during callback");
        check(count <= 64, ONEUI_VIEW_LIMIT, "Scope path depth");
        Runtime::ScopePath path;
        size_t bytes = 0;
        for (size_t i = 0; i < count; ++i) {
            const auto &segment = segments[i];
            for (const auto &s : {segment.instance_id, segment.key}) {
                check(s.length <= 1024 * 1024 - bytes, ONEUI_VIEW_LIMIT, "Scope path bytes");
                bytes += s.length;
            }
            auto decode = [](OneUiUtf8String s) {
                check(s.data || !s.length, ONEUI_VIEW_INVALID_ARGUMENT, "Null scope text");
                std::string result = s.length ? std::string(s.data, s.length) : std::string{};
                check(oneview::utf8(result), ONEUI_VIEW_INVALID_ARGUMENT, "Invalid scope UTF-8");
                return result;
            };
            auto id = decode(segment.instance_id), key = decode(segment.key);
            check(!id.empty() && !key.empty(), ONEUI_VIEW_INVALID_ARGUMENT, "Empty scope segment");
            path.emplace_back(std::move(id), std::move(key));
        }
        auto scope = value->rootScope;
        if (count) {
            auto found = value->renderScopes.find(path);
            check(found != value->renderScopes.end(), ONEUI_VIEW_NOT_FOUND, "Scope path not found");
            scope = found->second;
        }
        check(scope && !scope->closed && scope->notifier, ONEUI_VIEW_CLOSED, "Scope closed");
        oneui_view_notifier_retain(scope->notifier);
        *out = scope->notifier;
    });
}
