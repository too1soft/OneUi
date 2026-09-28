#pragma once
#include "oneui/developer_tools.h"
#include "oneui/widget.h"
#include <array>
#include <memory>

namespace oneui::internal {
class DiagnosticPaintSpan;
class ONEUI_API DeveloperRecorder {
public:
    explicit DeveloperRecorder(DeveloperOptions options);
    DeveloperOptions options;
    DeveloperSnapshot snapshot() const;
    const DeveloperSnapshot& summary() const { return state_; }
    const DeveloperIssue* latestIssue() const { return issueCount_ ? &issues_[(nextIssue_+issues_.size()-1)%issues_.size()] : nullptr; }
    void beginFrame(double nowMs);
    void finishFrame(double nowMs, bool animationPending, DeveloperFrame timing);
    void resetCadence() { lastPaintMs_=0; lastAnimating_=false; }
    void observeMotion(const Widget& widget, std::uint64_t revision, float progress, bool opening);
    void observePaint(const Widget& widget, double selfMs);
    DiagnosticPaintSpan* top=nullptr;
private:
    struct Motion {
        const Widget* widget=nullptr;
        std::weak_ptr<int> lifetime;
        std::uint64_t revision=0, seen=0;
        float progress=0;
    };
    std::array<Motion,256> motions_{};
    std::array<DeveloperIssue,128> issues_{};
    std::size_t issueCount_=0, nextIssue_=0;
    DeveloperSnapshot state_;
    DeveloperFrame current_;
    double startMs_=0, lastPaintMs_=0;
    bool lastAnimating_=false;
    void issue(unsigned flags, DiagnosticSource source, DeveloperFrame timing, bool inProgress=false);
};
ONEUI_API DeveloperRecorder* activeDeveloperRecorder();
class ONEUI_API DiagnosticSession {
    DeveloperRecorder* previous_;
public:
    explicit DiagnosticSession(DeveloperRecorder* recorder);
    ~DiagnosticSession();
    DiagnosticSession(const DiagnosticSession&)=delete;
};
class ONEUI_API DiagnosticPaintSpan {
    DeveloperRecorder* recorder_;
    const Widget& widget_;
    DiagnosticPaintSpan* parent_=nullptr;
    double start_=0, childrenMs_=0;
public:
    explicit DiagnosticPaintSpan(const Widget& widget);
    ~DiagnosticPaintSpan();
    DiagnosticPaintSpan(const DiagnosticPaintSpan&)=delete;
};
}
