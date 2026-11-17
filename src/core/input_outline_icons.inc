case IconSymbol::OutlineEye:
case IconSymbol::OutlineEyeOff: {
    CanvasPath path;
    path.moveTo(p(2,12));
    path.cubicTo(p(4,7),p(7,5),p(12,5));
    path.cubicTo(p(17,5),p(20,7),p(22,12));
    path.cubicTo(p(20,17),p(17,19),p(12,19));
    path.cubicTo(p(7,19),p(4,17),p(2,12));
    path.close();
    canvas.strokePath(path,color,stroke,true);
    canvas.strokeEllipse(r(9,9,6,6),color,stroke);
    if (symbol == IconSymbol::OutlineEyeOff)
        canvas.drawLine(p(3,3),p(21,21),color,stroke);
    return true;
}
