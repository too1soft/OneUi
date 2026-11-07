// Standard outline glyphs from the approved settings prototype DOM; 24-unit viewBox.
case IconSymbol::OutlineSlidersHorizontal: {
    const float width = 1.75f * std::min(rect.width, rect.height) / 24.0f;
    { CanvasPath path;
      path.moveTo(p(21.000000f,4.000000f));
      path.lineTo(p(14.000000f,4.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(10.000000f,4.000000f));
      path.lineTo(p(3.000000f,4.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(21.000000f,12.000000f));
      path.lineTo(p(12.000000f,12.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(8.000000f,12.000000f));
      path.lineTo(p(3.000000f,12.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(21.000000f,20.000000f));
      path.lineTo(p(16.000000f,20.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(12.000000f,20.000000f));
      path.lineTo(p(3.000000f,20.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(14.000000f,2.000000f));
      path.lineTo(p(14.000000f,6.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(8.000000f,10.000000f));
      path.lineTo(p(8.000000f,14.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(16.000000f,18.000000f));
      path.lineTo(p(16.000000f,22.000000f));
      canvas.strokePath(path,color,width,true); }
    return true;
}
case IconSymbol::OutlineKeyboard: {
    const float width = 1.75f * std::min(rect.width, rect.height) / 24.0f;
    { CanvasPath path;
      path.moveTo(p(10.000000f,8.000000f));
      path.lineTo(p(10.010000f,8.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(12.000000f,12.000000f));
      path.lineTo(p(12.010000f,12.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(14.000000f,8.000000f));
      path.lineTo(p(14.010000f,8.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(16.000000f,12.000000f));
      path.lineTo(p(16.010000f,12.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(18.000000f,8.000000f));
      path.lineTo(p(18.010000f,8.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(6.000000f,8.000000f));
      path.lineTo(p(6.010000f,8.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(7.000000f,16.000000f));
      path.lineTo(p(17.000000f,16.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(8.000000f,12.000000f));
      path.lineTo(p(8.010000f,12.000000f));
      canvas.strokePath(path,color,width,true); }
    canvas.strokeRect(r(2.000000f,4.000000f,20.000000f,16.000000f),color,2.000000f*rect.width/24.0f,width);
    return true;
}
case IconSymbol::OutlineShieldCheck: {
    const float width = 1.75f * std::min(rect.width, rect.height) / 24.0f;
    { CanvasPath path;
      path.moveTo(p(20.000000f,13.000000f));
      path.cubicTo(p(20.000000f,18.000000f),p(16.500000f,20.500000f),p(12.340000f,21.950000f));
      path.cubicTo(p(12.122162f,22.023817f),p(11.885537f,22.020286f),p(11.670000f,21.940000f));
      path.cubicTo(p(7.500000f,20.500000f),p(4.000000f,18.000000f),p(4.000000f,13.000000f));
      path.lineTo(p(4.000000f,6.000000f));
      path.cubicTo(p(4.000000f,5.447715f),p(4.447715f,5.000000f),p(5.000000f,5.000000f));
      path.cubicTo(p(7.000000f,5.000000f),p(9.500000f,3.800000f),p(11.240000f,2.280000f));
      path.cubicTo(p(11.677674f,1.906067f),p(12.322326f,1.906067f),p(12.760000f,2.280000f));
      path.cubicTo(p(14.510000f,3.810000f),p(17.000000f,5.000000f),p(19.000000f,5.000000f));
      path.cubicTo(p(19.552285f,5.000000f),p(20.000000f,5.447715f),p(20.000000f,6.000000f));
      path.lineTo(p(20.000000f,13.000000f));
      path.close();
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(9.000000f,12.000000f));
      path.lineTo(p(11.000000f,14.000000f));
      path.lineTo(p(15.000000f,10.000000f));
      canvas.strokePath(path,color,width,true); }
    return true;
}
case IconSymbol::OutlineCloud: {
    const float width = 1.75f * std::min(rect.width, rect.height) / 24.0f;
    { CanvasPath path;
      path.moveTo(p(17.500000f,19.000000f));
      path.lineTo(p(9.000000f,19.000000f));
      path.cubicTo(p(5.395701f,18.999075f),p(2.381268f,16.261406f),p(2.034305f,12.673845f));
      path.cubicTo(p(1.687342f,9.086284f),p(4.121280f,5.821688f),p(7.658602f,5.130078f));
      path.cubicTo(p(11.195925f,4.438468f),p(14.680200f,6.545946f),p(15.710000f,10.000000f));
      path.lineTo(p(17.500000f,10.000000f));
      path.cubicTo(p(19.985281f,10.000000f),p(22.000000f,12.014719f),p(22.000000f,14.500000f));
      path.cubicTo(p(22.000000f,16.985281f),p(19.985281f,19.000000f),p(17.500000f,19.000000f));
      path.close();
      canvas.strokePath(path,color,width,true); }
    return true;
}
case IconSymbol::OutlineInfo: {
    const float width = 1.75f * std::min(rect.width, rect.height) / 24.0f;
    canvas.strokeEllipse(r(2.000000f,2.000000f,20.000000f,20.000000f),color,width);
    { CanvasPath path;
      path.moveTo(p(12.000000f,16.000000f));
      path.lineTo(p(12.000000f,12.000000f));
      canvas.strokePath(path,color,width,true); }
    { CanvasPath path;
      path.moveTo(p(12.000000f,8.000000f));
      path.lineTo(p(12.010000f,8.000000f));
      canvas.strokePath(path,color,width,true); }
    return true;
}
