#include "oneui/controls/dialog.h"
#include "oneui/controls/button.h"
void dialogCaptionContract() {
 auto dialog=std::make_shared<oneui::Dialog>(L"Developer",L"Inspect a folder");
 auto sheet=std::make_shared<oneui::StyleSheet>();std::string error;
 const auto css=oneui::ui::syntax::css("Dialog.local-caption {padding:16px 18px;font-size:15px;font-weight:650;color:#e8eaee;subtitle-font-size:12px;subtitle-color:#a3a6ab;}");
 expect(sheet->addRulesFromCss(css,&error),"caption styles parse");
 dialog->setStyleSheet(sheet,{"dialog",{"one-Dialog","local-caption"},oneui::StyleStateNone});
 dialog->setFrame({10,20,620,380});
 oneui::ui::Mount properties({},std::make_shared<oneui::ui::StyleRegistry>(),{});
 auto element=oneui::ui::Element(oneui::ui::Node(dialog),"Dialog");
 properties.set(element,"header-text-layout",std::wstring(L"0 21.75 23.75 17.390625"));
 const auto parts=dialog->textParts();
 expect(parts[0].text==L"Developer" && parts[0].frame.x==28 && parts[0].frame.y==36,"exported title is the actual caption and authored line box");
 expect(parts[0].fontSize==15 && parts[0].fontWeight==650,"actual title typography is inspected");
 expect(parts[1].fontSize==12 && parts[1].fontWeight==400 && parts[1].color.r==163,"subtitle consumes the dialog's explicit local typography");
 oneui::test_support::RecordingCanvas canvas;dialog->paint(canvas);
 expect(canvas.texts.size()>=2 && canvas.texts[0].weight==parts[0].fontWeight && canvas.texts[0].rect.y==parts[0].frame.y,"paint and inspection share the title metrics");
 try {properties.set(element,"header-text-layout",std::wstring(L"0 0 2 3"));expect(false,"zero height rejected");}catch(const std::invalid_argument&){}
 expect(dialog->textParts()[0].frame.height==parts[0].frame.height,"invalid metrics preserve previous caption geometry");
 oneui::Button button(L"Actual caption");button.setAccessibleName(L"Different accessibility name");button.setFrame({0,0,200,32});
 oneui::test_support::RecordingCanvas buttonCanvas;button.paint(buttonCanvas);
 const auto style=button.currentPaintStyle();
 expect(!buttonCanvas.texts.empty() && buttonCanvas.texts[0].text==button.text() && buttonCanvas.texts[0].weight==style.fontWeight && buttonCanvas.texts[0].size==style.fontSize,"button inspection reports actual caption paint, never its accessibility name");
 auto iconButton=std::make_shared<oneui::Button>(L"Discover");
 iconButton->setIcon(oneui::IconSymbol::OutlineCompass);iconButton->setFrame({0,0,200,32});
 oneui::ui::Element iconElement(oneui::ui::Node(iconButton),"Button");iconElement.classes="local-icon";
 oneui::ui::StyleRegistry registry;registry.add(iconElement);
 registry.replace(oneui::ui::syntax::css("Button.local-icon {color:#e8eaee;icon-color:#6e9bff;}"));
 oneui::test_support::RecordingCanvas iconCanvas;iconButton->paint(iconCanvas);
 expect(!iconCanvas.strokeEllipses.empty() && iconCanvas.strokeEllipses[0].color.b==255 && iconCanvas.strokeEllipses[0].color.r==110,"button icon consumes its own authored color");
 expect(!iconCanvas.texts.empty() && iconCanvas.texts.back().color.r==232,"icon color preserves actual caption foreground");
 registry.replace(oneui::ui::syntax::css("Button.local-icon {color:#e8eaee;icon-color:#6e9bff;title-offset-y:-1px;}"));
 oneui::test_support::RecordingCanvas offsetCanvas;iconButton->paint(offsetCanvas);
 expect(offsetCanvas.texts.back().rect.y==iconCanvas.texts.back().rect.y-1,"caption offset reaches actual paint");
 expect(offsetCanvas.strokeEllipses[0].rect.y==iconCanvas.strokeEllipses[0].rect.y,"caption correction leaves icon position unchanged");
 registry.replace(oneui::ui::syntax::css("Button.local-icon {color:#a7aebb;}"));
 oneui::test_support::RecordingCanvas resetCanvas;iconButton->paint(resetCanvas);
 expect(resetCanvas.strokeEllipses[0].color.r==167,"removing icon color restores caption color inheritance");
 expect(resetCanvas.texts.back().rect.y==iconCanvas.texts.back().rect.y,"removing caption offset restores its original baseline");
 iconButton->setTrailingText(L"9");iconButton->setIconColor(oneui::Color{110,155,255});
 oneui::test_support::RecordingCanvas countCanvas;iconButton->paint(countCanvas);
 expect(countCanvas.texts.size()==2 && countCanvas.texts.back().color.r==167,"independent icon ink never recolors trailing count text");
 iconButton->setIconColor(oneui::Color{110,155,255});iconButton->setDisabled(true);
 oneui::test_support::RecordingCanvas disabledCanvas;iconButton->paint(disabledCanvas);
 expect(disabledCanvas.strokeEllipses[0].color.r==disabledCanvas.texts.back().color.r,"disabled icon follows disabled caption ink");
}
