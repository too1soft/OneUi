// Real IR -> record adapter -> native chart -> inspection event.
#include <cmath>
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
 expect(chart->series().size()==1 && chart->series()[0].values[0]==37.5 && std::isnan(chart->series()[0].values[1]),"missing samples remain gaps, never zero");
 chart->setFrame({0,0,240,82});
 oneui::MouseEvent move;move.position={60,30};chart->onMouseMove(move);
 expect(c.vm.invokes==1 && c.vm.invokedArgs.size()==2 && c.vm.invokedArgs[0].integer==0 && !c.vm.invokedArgs[1].integer,"native hover returns index and pin state");
 oneui::test_support::RecordingCanvas canvas;chart->paint(canvas);
 expect(canvas.lines.size()>=3 && std::abs(canvas.lines[0].width-0.7f)<0.001f && canvas.lines[0].from.y==canvas.lines[0].to.y,"chart paints authored horizontal grid");
 c.runtime->close();expect(series.retains==series.releases && point.retains==point.releases && missing.retains==missing.releases,"chart nested record leases balance");
}
