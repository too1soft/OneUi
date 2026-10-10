#pragma once
// Opt-in native timing instrumentation. Normal builds compile every probe away.
// Exclusive nanoseconds: nested probes subtract their entire duration from the
// parent. One CSV record per outer operation; no per-row I/O on the hot path.
#ifdef ONEUI_VIEW_PROFILE
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
namespace oneui::internal::view_profile {
enum Stage { Flush, Delta, Vm, Conversion, Rows, Keyed, Layout, Paint, Count };
struct Probe;
struct State {
    Probe* top = nullptr;
    std::array<long long, Count> ns{}, calls{};
    FILE* file = nullptr;
    void open() { if (!file) if (auto path = std::getenv("ONEUI_VIEW_PROFILE")) file = std::fopen(path, "a"); }
    ~State() { if (file) std::fclose(file); }
};
inline thread_local State state;
struct Probe {
    using Clock = std::chrono::steady_clock;
    Stage stage; Probe* parent; Clock::time_point start; long long children = 0;
    Probe(Stage stage) : stage(stage), parent(state.top), start(Clock::now()) { state.top = this; }
    ~Probe() {
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
        state.ns[stage] += elapsed-children; ++state.calls[stage]; state.top = parent;
        if (parent) parent->children += elapsed;
        else {
            constexpr const char* names[] = {"flush","delta","vm","conversion","rows","keyed","layout","paint"};
            state.open();
            char label[128]{}; GetEnvironmentVariableA("ONEUI_PROFILE_CASE",label,sizeof(label));
            if (state.file) for (int i=0;i<Count;++i) if (state.calls[i])
                std::fprintf(state.file,"%s,%s,%s,%lld,%lld\n",label[0] ? label : "init",names[stage],names[i],state.ns[i],state.calls[i]);
            state.ns.fill(0); state.calls.fill(0);
        }
    }
};
}
#define ONEUI_PROFILE_JOIN_(a,b) a##b
#define ONEUI_PROFILE_JOIN(a,b) ONEUI_PROFILE_JOIN_(a,b)
#define ONEUI_VIEW_TIME(stage) ::oneui::internal::view_profile::Probe ONEUI_PROFILE_JOIN(viewProbe,__LINE__)(::oneui::internal::view_profile::stage)
#else
#define ONEUI_VIEW_TIME(stage) ((void)0)
#endif
