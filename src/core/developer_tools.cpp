#include "internal/developer_tools.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace oneui {
namespace {
double nowMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::string quoted(const std::string& text) {
    std::ostringstream out; out << '"';
    for(unsigned char c:text) {
        if(c=='"' || c=='\\')out << '\\' << c;
        else if(c<32)out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c);
        else out << c;
    }
    return out.str()+'"';
}
void sourceJson(std::ostream& out,const DiagnosticSource& source) {
    out << "{\"component\":" << quoted(source.component) << ",\"file\":" << quoted(source.file) << ",\"line\":" << source.line << '}';
}
void frameJson(std::ostream& out,const DeveloperFrame& f) {
    out << "{\"paint_ms\":" << f.paintMs << ",\"interval_ms\":" << f.intervalMs
        << ",\"content_ms\":" << f.contentMs << ",\"submit_ms\":" << f.submitMs << ",\"blit_ms\":" << f.blitMs
        << ",\"layout_ms\":" << f.layoutMs << ",\"text_layout_ms\":" << f.textLayoutMs
        << ",\"fill_ms\":" << f.fillMs << ",\"path_ms\":" << f.pathMs
        << ",\"hottest_self_ms\":" << f.hottestSelfMs << ",\"hottest\":";
    sourceJson(out,f.hottest);out << '}';
}
}
std::string developerReportJson(const DeveloperSnapshot& s) {
    std::ostringstream out;out.imbue(std::locale::classic());out << std::setprecision(8);
    out << "{\"schema\":1,\"supported\":" << (s.supported?"true":"false") << ",\"enabled\":" << (s.enabled?"true":"false")
        << ",\"backend\":" << quoted(s.backend) << ",\"overlay\":" << (s.overlay?"true":"false") << ",\"frame_budget_ms\":" << s.frameBudgetMs << ",\"frames\":" << s.frames
        << ",\"slow_paints\":" << s.slowPaints << ",\"animation_delays\":" << s.animationDelays
        << ",\"motion_reversals\":" << s.motionReversals << ",\"discarded_issues\":" << s.discardedIssues
        << ",\"motion_samples\":" << s.motionSamples << ",\"motion_evictions\":" << s.motionEvictions << ",\"last\":";frameJson(out,s.last);out << ",\"issues\":[";
    bool first=true;
    for(const auto& issue:s.issues) {
        if(!first)out << ',';first=false;
        out << "{\"frame\":" << issue.frame << ",\"flags\":" << issue.flags << ",\"source\":";
        sourceJson(out,issue.source);out << ",\"timing\":";frameJson(out,issue.timing);out << '}';
    }
    out << "]}\n";return out.str();
}
namespace internal {
namespace { thread_local DeveloperRecorder* active=nullptr; }
DeveloperRecorder* activeDeveloperRecorder(){return active;}
DiagnosticSession::DiagnosticSession(DeveloperRecorder* recorder):previous_(active){active=recorder;}
DiagnosticSession::~DiagnosticSession(){active=previous_;}
DeveloperRecorder::DeveloperRecorder(DeveloperOptions value):options(value) {
    if(!std::isfinite(value.frameBudgetMs) || value.frameBudgetMs<=0)throw std::invalid_argument("Developer frame budget must be finite and positive");
    state_.supported=true;state_.enabled=true;state_.frameBudgetMs=value.frameBudgetMs;
}
DeveloperSnapshot DeveloperRecorder::snapshot()const {
    auto result=state_;result.overlay=options.overlay;result.issues.reserve(issueCount_);
    for(std::size_t i=0;i<issueCount_;++i)result.issues.push_back(issues_[(nextIssue_+issues_.size()-issueCount_+i)%issues_.size()]);
    return result;
}
void DeveloperRecorder::beginFrame(double now) {startMs_=now;current_={};}
void DeveloperRecorder::issue(unsigned flags,DiagnosticSource source,DeveloperFrame timing,bool inProgress) {
    if(issueCount_==issues_.size())++state_.discardedIssues;else ++issueCount_;
    issues_[nextIssue_]={state_.frames+(inProgress?1:0),flags,std::move(timing),std::move(source)};
    nextIssue_=(nextIssue_+1)%issues_.size();
}
void DeveloperRecorder::finishFrame(double now,bool animationPending,DeveloperFrame timing) {
    ++state_.frames;
    timing.paintMs=now-startMs_;
    timing.intervalMs=lastAnimating_ && lastPaintMs_>0 ? now-lastPaintMs_ : 0;
    timing.hottestSelfMs=current_.hottestSelfMs;timing.hottest=std::move(current_.hottest);
    unsigned flags=0;
    if(timing.paintMs>options.frameBudgetMs*1.5){++state_.slowPaints;flags|=SlowPaint;}
    if(timing.intervalMs>options.frameBudgetMs*1.5){++state_.animationDelays;flags|=AnimationDelay;}
    if(flags) {
        auto source=timing.hottest;
        // An expensive submit/blit or a gap outside paint must not be blamed on
        // an otherwise cheap widget. Keep hottest-widget evidence in timing.
        if(!(flags&SlowPaint))source={"Window.animationCadence",{},0};
        else if(timing.submitMs>timing.contentMs && timing.submitMs>=timing.blitMs)source={"Window.rendererSubmit",{},0};
        else if(timing.blitMs>timing.contentMs)source={"Window.softwareBlit",{},0};
        issue(flags,std::move(source),timing);
    }
    state_.last=std::move(timing);lastPaintMs_=now;lastAnimating_=animationPending;
}
void DeveloperRecorder::observePaint(const Widget& widget,double selfMs) {
    if(selfMs>current_.hottestSelfMs) {current_.hottestSelfMs=selfMs;current_.hottest=widget.diagnosticSource();}
}
void DeveloperRecorder::observeMotion(const Widget& widget,std::uint64_t revision,float progress,bool opening) {
    ++state_.motionSamples;
    Motion* slot=nullptr;Motion* available=nullptr;
    for(auto& m:motions_) {
        if(m.widget==&widget && !m.lifetime.expired()){slot=&m;break;}
        if(!available && m.lifetime.expired())available=&m;
    }
    if(!slot) {
        if(!available){available=&*std::min_element(motions_.begin(),motions_.end(),[](const auto& a,const auto& b){return a.seen<b.seen;});++state_.motionEvictions;}
        *available={&widget,widget.lifetimeToken(),revision,state_.frames,progress};return;
    }
    // Gaps mean the widget was hidden or culled; do not join unrelated samples.
    if(slot->revision==revision && slot->seen+1>=state_.frames && (opening?1:-1)*(progress-slot->progress)<-.0001f) {
        ++state_.motionReversals;issue(MotionReversal,widget.diagnosticSource(),{},true);
    }
    slot->revision=revision;slot->progress=progress;slot->seen=state_.frames;
}
DiagnosticPaintSpan::DiagnosticPaintSpan(const Widget& widget):recorder_(active),widget_(widget) {
    if(recorder_){parent_=recorder_->top;recorder_->top=this;start_=nowMs();}
}
DiagnosticPaintSpan::~DiagnosticPaintSpan() {
    if(!recorder_)return;
    const double elapsed=nowMs()-start_;
    recorder_->observePaint(widget_,std::max(0.0,elapsed-childrenMs_));
    if(parent_)parent_->childrenMs_+=elapsed;
    recorder_->top=parent_;
}
}
}
