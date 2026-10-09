#include "lab_options.hpp"
#include "demo_catalog.hpp"

Options parseLabOptions(const std::vector<std::wstring> &arguments, bool &selfTest) {
    Options o;
    selfTest = false;
    bool explicitPage = false;
    for (int i = 1; i < static_cast<int>(arguments.size()); ++i) {
        std::wstring arg = arguments[i];
        if (arg == L"--view" || arg == L"--benchmark-seconds" || arg == L"--renderer-benchmark" ||
            arg == L"--stability-seconds" || arg == L"--connections-stress" ||
            arg == L"--editor-idle-seconds" || arg == L"--load" ||
            arg == L"--particle-mode" || arg == L"--warm-editor")
            o.legacy = true;
        auto next = [&]() -> std::wstring {
            if (i + 1 >= static_cast<int>(arguments.size()))
                throw std::runtime_error("Missing argument value");
            return arguments[++i];
        };
        if (arg == L"--oneui-notification-server" || arg == L"-Embedding" || arg == L"/Embedding")
            continue;
        if (arg == L"--page") {
            auto value = next();
            o.page = std::string(value.begin(), value.end());
            explicitPage = true;
            if (!lab::findPage(o.page))
                throw std::runtime_error("Unknown page ID");
        } else if (arg == L"--legacy-lab")
            o.legacy = true;
        else if (arg == L"--renderer") {
            const auto value = next();
            if (value != L"auto" && value != L"gpu" && value != L"cpu")
                throw std::runtime_error("Renderer must be auto, gpu or cpu");
            o.renderer = std::string(value.begin(), value.end());
        } else if (arg == L"--particle-mode") {
            const auto value = next();
            if (value == L"reference")
                o.particleMode = ParticleMode::Reference;
            else if (value == L"precomputed")
                o.particleMode = ParticleMode::Precomputed;
            else if (value == L"batch")
                o.particleMode = ParticleMode::Batch;
            else if (value == L"combined")
                o.particleMode = ParticleMode::Combined;
            else if (value == L"mesh")
                o.particleMode = ParticleMode::Mesh;
            else
                throw std::runtime_error(
                    "Particle mode must be reference, precomputed, batch, combined or mesh");
        } else if (arg == L"--renderer-benchmark") {
            const auto value = next();
            if (value != L"idle" && value != L"table" && value != L"chart" &&
                value != L"particles" && value != L"effects" && value != L"effects-idle")
                throw std::runtime_error("Invalid renderer benchmark scene");
            o.rendererBenchmark = std::string(value.begin(), value.end());
        } else if (arg == L"--sample-seconds")
            o.sampleSeconds = std::stod(next());
        else if (arg == L"--stability-seconds")
            o.stabilitySeconds = std::stod(next());
        else if (arg == L"--stability-exercise")
            o.stabilityExercise = true;
        else if (arg == L"--load") {
            auto s = next();
            if (s != L"light" && s != L"medium" && s != L"heavy")
                throw std::runtime_error("Invalid load");
            o.load = s == L"light" ? 0 : s == L"heavy" ? 2 : 1;
        } else if (arg == L"--view") {
            auto s = next();
            if (s != L"overview" && s != L"chart" && s != L"particles" && s != L"list" &&
                s != L"editor" && s != L"connections" && s != L"details" && s != L"components")
                throw std::runtime_error("Invalid view");
            o.tab = s == L"chart" ? 1 : s == L"particles" ? 2 : s == L"list" ? 3 : 0;
            o.connections = s == L"connections";
            o.details = s == L"details";
            o.gallery = s == L"components";
            o.editor = s == L"editor" || o.connections || o.details || o.gallery;
        } else if (arg == L"--benchmark-seconds") {
            o.benchmark = std::stod(next());
            if (!std::isfinite(o.benchmark) || o.benchmark <= 0 || o.benchmark > 3600)
                throw std::runtime_error("Invalid duration");
        } else if (arg == L"--width")
            o.width = std::max(640, std::stoi(next()));
        else if (arg == L"--height")
            o.height = std::max(560, std::stoi(next()));
        else if (arg == L"--warm-editor")
            o.warmEditor = true;
        else if (arg == L"--editor-light")
            o.editorLight = true;
        else if (arg == L"--editor-dark")
            o.editorDark = true;
        else if (arg == L"--editor-invalid")
            o.editorInvalid = true;
        else if (arg == L"--connection-query")
            o.connectionQuery = next();
        else if (arg == L"--entry") {
            auto value = next();
            if (value != L"code" && value != L"template")
                throw std::runtime_error("Invalid entry");
            o.templateEntry = value == L"template";
        } else if (arg == L"--dev")
            o.dev = true;
        else if (arg == L"--devtools")
            o.devtools = true;
        else if (arg == L"--compact")
            o.compact = true;
        else if (arg == L"--layout-settings")
            o.layoutSettings = true;
        else if (arg == L"--layout-detail")
            o.layoutDetail = true;
        else if (arg == L"--soft-preset")
            o.softPreset = true;
        else if (arg == L"--component-scene") {
            o.galleryScene = std::stoi(next());
            if (o.galleryScene < 0 || o.galleryScene > 4)
                throw std::runtime_error("Component scene must be 0, 1, 2, 3 or 4");
        } else if (arg == L"--css")
            o.css = std::filesystem::absolute(next());
        else if (arg == L"--connections-stress")
            o.connectionStress = std::stoi(next());
        else if (arg == L"--scale")
            o.scale = std::stof(next());
        else if (arg == L"--editor-idle-seconds")
            o.editorIdle = std::stod(next());
        else if (arg == L"--exit-after-benchmark")
            o.exitAfter = true;
        else if (arg == L"--snapshot")
            o.snapshot = std::filesystem::absolute(next());
        else if (arg == L"--snapshot-exit")
            o.snapshotExit = true;
        else if (arg == L"--output")
            o.output = std::filesystem::absolute(next());
        else if (arg == L"--desktop-menus")
            o.desktopMenus = true;
        else if (arg == L"--trace-input")
            o.traceInput = true;
        else if (arg == L"--self-test")
            selfTest = true;
        else
            throw std::runtime_error("Unknown argument");
    }
    if (!std::isfinite(o.scale) || o.scale < 1 || o.scale > 2 || !std::isfinite(o.editorIdle) ||
        o.editorIdle < 0 || o.editorIdle > 60)
        throw std::runtime_error("Invalid diagnostic options");
    if (o.connectionStress < 0 || o.connectionStress > 3000 ||
        (o.connectionStress &&
         (!o.connections || !o.snapshot.empty() || o.editorIdle > 0 || !o.connectionQuery.empty())))
        throw std::runtime_error("Use --view connections for standalone stress diagnostics");
    if (o.legacy && !o.editor) {
        o.width = std::max(1050, o.width);
        o.height = std::max(720, o.height);
    }
    if (o.editor && o.benchmark > 0)
        throw std::runtime_error("Use --editor-idle-seconds for the event-driven editor");
    if (o.benchmark > 0 && !o.snapshot.empty())
        throw std::runtime_error("Capture and benchmark must run separately");
    if (!std::isfinite(o.sampleSeconds) || o.sampleSeconds < 1 || o.sampleSeconds > 60)
        throw std::runtime_error("Sample seconds must be 1..60");
    if (!o.rendererBenchmark.empty()) {
        if (o.benchmark > 0 || o.dev || !o.snapshot.empty() || o.editorIdle > 0 ||
            o.connectionStress > 0)
            throw std::runtime_error(
                "Renderer benchmark must run standalone with dev and capture disabled");
        o.editor = o.connections = o.rendererBenchmark == "idle" || o.rendererBenchmark == "table";
        o.details = o.gallery = false;
        if (o.rendererBenchmark == "effects" || o.rendererBenchmark == "effects-idle") {
            o.editor = o.gallery = true;
            o.galleryScene = 3;
        }
        o.tab = o.rendererBenchmark == "chart" ? 1 : o.rendererBenchmark == "particles" ? 2 : 0;
    }
    if (!std::isfinite(o.stabilitySeconds) || o.stabilitySeconds < 0 || o.stabilitySeconds > 3600 ||
        (o.stabilityExercise && o.stabilitySeconds < 20))
        throw std::runtime_error("Invalid stability duration");
    if (o.stabilitySeconds > 0) {
        if (o.benchmark > 0 || !o.rendererBenchmark.empty() || o.dev || !o.snapshot.empty() ||
            o.editorIdle > 0 || o.connectionStress > 0)
            throw std::runtime_error("Stability diagnostics must run standalone");
        o.editor = o.connections = o.details = o.gallery = false;
        o.tab = 2;
    }
    if (explicitPage && o.legacy)
        throw std::runtime_error("--page cannot be combined with legacy/benchmark scene options");
    return o;
}
