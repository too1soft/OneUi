#pragma once
#include "plots.hpp"

struct Options {
    bool embedded = false, legacy = false;
    std::string page = "layout/basic";
    int load = 1, tab = 0, width = 1320, height = 900;
    double benchmark = 0;
    std::string renderer = "auto", rendererBenchmark;
    ParticleMode particleMode = DEFAULT_PARTICLE_MODE;
    double sampleSeconds = 5, stabilitySeconds = 0;
    bool stabilityExercise = false, desktopMenus = false;
    bool exitAfter = false, snapshotExit = false, traceInput = false;
    bool layoutSettings = false, layoutDetail = false, softPreset = false;
    bool editor = false, warmEditor = false, editorLight = false, editorDark = false,
         editorInvalid = false;
    bool connections = false, details = false, gallery = false, compact = false;
    int galleryScene = 0;
    bool templateEntry = false, dev = false, devtools = false;
    std::filesystem::path css = std::filesystem::path(__FILE__).parent_path() / "connections.css";
    int connectionStress = 0;
    std::wstring connectionQuery;
    double editorIdle = 0;
    float scale = 1;
    std::filesystem::path output = std::filesystem::path(__FILE__).parent_path() / "results",
                          snapshot;
};
Options parseLabOptions(const std::vector<std::wstring> &arguments, bool &selfTest);
