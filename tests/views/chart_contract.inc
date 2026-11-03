// Real IR -> record adapter -> native chart -> inspection event.
#include <cmath>
#include "../../src/core/time_series_curve.h"
void chartContract(const char* file) {
 Case c(file);
 Vm series(c.image,"ChartSeries"),point(c.image,"ChartPoint"),missing(c.image,"ChartPoint");
 series.text(series.key,"cpu");series.text(id(c.image,"ChartSeries","name"),"CPU");
 series.values[id(c.image,"ChartSeries","rgba")].integer=0x319beaff;
 point.text(point.key,"0");point.values[id(c.image,"ChartPoint","value")].number=37.5;point.values[id(c.image,"ChartPoint","valid")].integer=1;
 missing.text(missing.key,"1");missing.values[id(c.image,"ChartPoint","value")].number=99;
 series.items={&point,&missing};c.vm.lists[id(c.image,"Root","series")]={&series};
 c.start();auto chart=c.runtime->root->children.back()->children.back()->element.as<oneui::TimeSeriesChart>();
 expect(chart->minimum()==0 && chart->maximum()==100 && chart->gridLines()==3,"authored range/grid reach native chart");
 expect(chart->thresholds().size()==1 && chart->thresholds()[0].value==80 && chart->thresholds()[0].color.a==140,"authored thresholds reach the real native chart");
 expect(chart->thresholdDashLength()==2 && chart->thresholdGapLength()==4,"authored threshold dash style reaches native paint");
 expect(chart->series().size()==1 && chart->series()[0].values[0]==37.5 && std::isnan(chart->series()[0].values[1]),"missing samples remain gaps, never zero");
 chart->setFrame({0,0,240,82});
 oneui::MouseEvent move;move.position={60,30};chart->onMouseMove(move);
 expect(c.vm.invokes==1 && c.vm.invokedArgs.size()==2 && c.vm.invokedArgs[0].integer==0 && !c.vm.invokedArgs[1].integer,"native hover returns index and pin state");
 oneui::test_support::RecordingCanvas canvas;chart->paint(canvas);
 expect(canvas.lines.size()>=3 && std::abs(canvas.lines[0].width-0.7f)<0.001f && canvas.lines[0].from.y==canvas.lines[0].to.y,"chart paints authored horizontal grid");
 bool thresholdPainted=false;
 for(const auto& line:canvas.lines) if(line.color.a==140 && std::abs(line.to.x-line.from.x-2.0f)<0.001f) thresholdPainted=true;
 expect(thresholdPainted,"native threshold renders the authored two-pixel dash");
 oneui::ui::Mount properties({},std::make_shared<oneui::ui::StyleRegistry>(),{});
 auto thresholdElement=oneui::ui::Element(oneui::ui::Node(chart),"TimeSeriesChart");
 try {properties.set(thresholdElement,"thresholds",std::wstring(L"not-a-number 1 2 3 4"));expect(false,"invalid threshold must fail");} catch(const std::invalid_argument&) {}
 expect(chart->thresholds().size()==1,"invalid input preserves the previous threshold");
 properties.set(thresholdElement,"thresholds",std::wstring{});
 expect(chart->thresholds().empty(),"clearing the binding removes stale threshold lines");
 properties.set(thresholdElement,"latest-style",std::wstring(L"0 7 7 2"));
 properties.set(thresholdElement,"secondary-style",std::wstring(L"1.2 0.85"));
 expect(chart->latestPointSeries()==0 && chart->latestPointWidth()==7 && chart->latestPointOutline()==2,"authored primary marker style reaches the chart");
 expect(std::abs(chart->secondarySeriesWidth()-1.2f)<0.001f && std::abs(chart->secondarySeriesOpacity()-0.85f)<0.001f,"secondary line width and opacity reach paint");
 chart->setSeries({{L"primary",{10,20,30,255},{10,20,30},false,false},{L"secondary",{80,90,100,255},{1,2,3},false,false}});
 chart->setLatestPointVisible(true); chart->setInspectionIndex(-1);
 oneui::StyleBox material;material.background.color=oneui::Color{38,41,46,255};chart->setStyleBox(material);
 oneui::test_support::RecordingCanvas styled;chart->paint(styled);
 expect(!styled.fillRects.empty() && styled.fillRects.front().color.r==38,"flat chart material paints the explicitly authored background");
 expect(styled.fillEllipses.size()==2 && styled.fillEllipses[0].rect.width==11 && styled.fillEllipses[1].rect.width==7,"only the primary endpoint paints a seven-pixel dot and two-pixel outline");
 expect(styled.fillEllipses[0].color.r==38 && styled.fillEllipses[0].color.g==41,"endpoint outline uses the same chart material");
 const auto harmonic=oneui::chart_curve::tangents({{0,0},{1,1},{2,5}});
 expect(std::abs(harmonic[1]-1.6f)<0.0001f,"monotone curve uses the weighted harmonic tangent, not the arithmetic mean");
 const auto extrema=oneui::chart_curve::tangents({{0,0},{1,2},{3,1},{4,1}});
 expect(extrema[1]==0 && extrema[2]==0 && extrema[3]==0,"uneven spacing, extrema and flat runs do not overshoot");
 c.runtime->close();expect(series.retains==series.releases && point.retains==point.releases && missing.retains==missing.releases,"chart nested record leases balance");
}
