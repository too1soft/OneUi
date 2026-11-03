// Actual captions/paint parameters; accessibility names remain independent.
void appendButtonCaption(std::ostringstream& stream,const oneui::Button& button) {
    const auto style=button.currentPaintStyle();
    appendJsonString(stream,utf8FromWide(button.text()));
    stream << ",\"paintFontSize\":";appendJsonNumber(stream,style.fontSize);
    stream << ",\"paintFontWeight\":" << style.fontWeight;
    stream << ",\"paintForeground\":";appendJsonColor(stream,style.foreground);
    stream << ",\"paintBackground\":";
    appendJsonColor(stream,style.gradient?std::optional<oneui::Color>{}:style.background);
}
void appendCaptionFrame(std::ostringstream& stream,oneui::Rect frame) {
    stream << "{\"x\":";appendJsonNumber(stream,frame.x);
    stream << ",\"y\":";appendJsonNumber(stream,frame.y);
    stream << ",\"width\":";appendJsonNumber(stream,frame.width);
    stream << ",\"height\":";appendJsonNumber(stream,frame.height);stream << '}';
}
void appendDialogParts(std::ostringstream& stream,const oneui::Dialog& dialog) {
    const auto parts=dialog.textParts();
    stream << ",\"parts\":{";
    for(std::size_t i=0;i<parts.size();++i) {
        if(i)stream << ',';
        appendJsonString(stream,i==0?"title":"subtitle");stream << ":{\"frame\":";
        appendCaptionFrame(stream,parts[i].frame);
        stream << ",\"text\":";appendJsonString(stream,utf8FromWide(parts[i].text));
        stream << ",\"paintFontSize\":";appendJsonNumber(stream,parts[i].fontSize);
        stream << ",\"paintFontWeight\":" << parts[i].fontWeight;
        stream << ",\"paintForeground\":";appendJsonColor(stream,parts[i].color);
        stream << '}';
    }
    stream << '}';
}
