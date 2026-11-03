// Approved Hosts prototype glyphs, 24-unit viewBox, using the shared outline renderer.
case IconSymbol::OutlineArchive: {
    canvas.strokeRect(r(2,3,20,5),color,rect.width/24.0f,stroke);
    CanvasPath path;
    path.moveTo(p(4,8));path.lineTo(p(4,19));
    path.cubicTo(p(4,20.1046f),p(4.8954f,21),p(6,21));path.lineTo(p(18,21));
    path.cubicTo(p(19.1046f,21),p(20,20.1046f),p(20,19));path.lineTo(p(20,8));
    path.moveTo(p(10,12));path.lineTo(p(14,12));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineHardDrive: {
    CanvasPath path;
    path.moveTo(p(2,12));path.lineTo(p(22,12));
    path.moveTo(p(5.45f,5.11f));path.lineTo(p(2,12));path.lineTo(p(2,18));
    path.cubicTo(p(2,19.1046f),p(2.8954f,20),p(4,20));path.lineTo(p(20,20));
    path.cubicTo(p(21.1046f,20),p(22,19.1046f),p(22,18));path.lineTo(p(22,12));
    path.lineTo(p(18.55f,5.11f));path.cubicTo(p(18.2113f,4.4326f),p(17.5183f,4),p(16.76f,4));
    path.lineTo(p(7.24f,4));path.cubicTo(p(6.4817f,4),p(5.7887f,4.4326f),p(5.45f,5.11f));path.close();
    path.moveTo(p(6,16));path.lineTo(p(6.01f,16));path.moveTo(p(10,16));path.lineTo(p(10.01f,16));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineGitBranch: {
    canvas.strokeEllipse(r(15,3,6,6),color,stroke);
    canvas.strokeEllipse(r(3,15,6,6),color,stroke);
    CanvasPath path;path.moveTo(p(6,3));path.lineTo(p(6,15));
    path.moveTo(p(18,9));path.cubicTo(p(18,13.9706f),p(13.9706f,18),p(9,18));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineAppWindow: {
    canvas.strokeRect(r(2,4,20,16),color,2.0f*rect.width/24.0f,stroke);
    CanvasPath path;path.moveTo(p(10,4));path.lineTo(p(10,8));path.moveTo(p(2,8));path.lineTo(p(22,8));
    path.moveTo(p(6,4));path.lineTo(p(6,8));canvas.strokePath(path,color,stroke,true);return true;
}

case IconSymbol::OutlineChevronsUpDown: {
    CanvasPath path; path.moveTo(p(8,9));path.lineTo(p(12,5));path.lineTo(p(16,9));
    path.moveTo(p(8,15));path.lineTo(p(12,19));path.lineTo(p(16,15));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineNetwork: {
    canvas.strokeRect(r(16,16,6,6),color,rect.width/24.0f,stroke);
    canvas.strokeRect(r(2,16,6,6),color,rect.width/24.0f,stroke);
    canvas.strokeRect(r(9,2,6,6),color,rect.width/24.0f,stroke);
    CanvasPath path;path.moveTo(p(5,16));path.lineTo(p(5,13));path.cubicTo(p(5,12.4477f),p(5.4477f,12),p(6,12));
    path.lineTo(p(18,12));path.cubicTo(p(18.5523f,12),p(19,12.4477f),p(19,13));path.lineTo(p(19,16));
    path.moveTo(p(12,12));path.lineTo(p(12,8));canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineClipboardPaste: {
    canvas.strokeRect(r(8,2,8,4),color,rect.width/24.0f,stroke);
    CanvasPath path;path.moveTo(p(8,4));path.lineTo(p(6,4));path.cubicTo(p(4.8954f,4),p(4,4.8954f),p(4,6));
    path.lineTo(p(4,20));path.cubicTo(p(4,21.1046f),p(4.8954f,22),p(6,22));path.lineTo(p(18,22));path.cubicTo(p(19.1046f,22),p(20,21.1046f),p(20,20));
    path.moveTo(p(16,4));path.lineTo(p(18,4));path.cubicTo(p(19.1046f,4),p(20,4.8954f),p(20,6));path.lineTo(p(20,8));
    path.moveTo(p(11,14));path.lineTo(p(21,14));path.moveTo(p(17,10));path.lineTo(p(21,14));path.lineTo(p(17,18));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlinePlugZap: {
    CanvasPath path;path.moveTo(p(6.3f,20.3f));path.cubicTo(p(7.2389f,21.2389f),p(8.7611f,21.2389f),p(9.7f,20.3f));
    path.lineTo(p(12,18));path.lineTo(p(6,12));path.lineTo(p(3.7f,14.3f));path.cubicTo(p(2.7611f,15.2389f),p(2.7611f,16.7611f),p(3.7f,17.7f));path.close();
    path.moveTo(p(2,22));path.lineTo(p(5,19));path.moveTo(p(7.5f,13.5f));path.lineTo(p(10,11));path.moveTo(p(10.5f,16.5f));path.lineTo(p(13,14));
    path.moveTo(p(18,3));path.lineTo(p(14,7));path.lineTo(p(20,7));path.lineTo(p(16,11));
    canvas.strokePath(path,color,stroke,true);return true;
}

// Lucide v0.468.0, the exact menu glyph paths used by the approved prototype.
case IconSymbol::OutlineActivity: {
    CanvasPath path;
    path.moveTo(p(22.0f,12.0f));
    path.lineTo(p(19.52f,12.0f));
    path.cubicTo(p(18.62181f,11.99808f),p(17.83251f,12.59517f),p(17.59f,13.46f));
    path.lineTo(p(15.24f,21.82f));
    path.cubicTo(p(15.20889f,21.92667f),p(15.11111f,22.0f),p(15.0f,22.0f));
    path.cubicTo(p(14.88889f,22.0f),p(14.79111f,21.92667f),p(14.76f,21.82f));
    path.lineTo(p(9.24f,2.18f));
    path.cubicTo(p(9.208889f,2.073333f),p(9.111111f,2.0f),p(9.0f,2.0f));
    path.cubicTo(p(8.888889f,2.0f),p(8.791111f,2.073333f),p(8.76f,2.18f));
    path.lineTo(p(6.41f,10.54f));
    path.cubicTo(p(6.168494f,11.40126f),p(5.384477f,11.99744f),p(4.49f,12.0f));
    path.lineTo(p(2.0f,12.0f));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineClipboardCopy: {
    canvas.strokeRect(r(8,2,8,4),color,1.0f*rect.width/24.0f,stroke);
    CanvasPath path;
    path.moveTo(p(8.0f,4.0f));
    path.lineTo(p(6.0f,4.0f));
    path.cubicTo(p(4.895431f,4.0f),p(4.0f,4.895431f),p(4.0f,6.0f));
    path.lineTo(p(4.0f,20.0f));
    path.cubicTo(p(4.0f,21.10457f),p(4.895431f,22.0f),p(6.0f,22.0f));
    path.lineTo(p(18.0f,22.0f));
    path.cubicTo(p(19.10457f,22.0f),p(20.0f,21.10457f),p(20.0f,20.0f));
    path.lineTo(p(20.0f,18.0f));
    path.moveTo(p(16.0f,4.0f));
    path.lineTo(p(18.0f,4.0f));
    path.cubicTo(p(19.10457f,4.0f),p(20.0f,4.895431f),p(20.0f,6.0f));
    path.lineTo(p(20.0f,10.0f));
    path.moveTo(p(21.0f,14.0f));
    path.lineTo(p(11.0f,14.0f));
    path.moveTo(p(15.0f,10.0f));
    path.lineTo(p(11.0f,14.0f));
    path.lineTo(p(15.0f,18.0f));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineCopyPlus: {
    canvas.strokeRect(r(8,8,14,14),color,2.0f*rect.width/24.0f,stroke);
    CanvasPath path;
    path.moveTo(p(15.0f,12.0f));
    path.lineTo(p(15.0f,18.0f));
    path.moveTo(p(12.0f,15.0f));
    path.lineTo(p(18.0f,15.0f));
    path.moveTo(p(4.0f,16.0f));
    path.cubicTo(p(2.9f,16.0f),p(2.0f,15.1f),p(2.0f,14.0f));
    path.lineTo(p(2.0f,4.0f));
    path.cubicTo(p(2.0f,2.9f),p(2.9f,2.0f),p(4.0f,2.0f));
    path.lineTo(p(14.0f,2.0f));
    path.cubicTo(p(15.1f,2.0f),p(16.0f,2.9f),p(16.0f,4.0f));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineFolderInput: {
    CanvasPath path;
    path.moveTo(p(2.0f,9.0f));
    path.lineTo(p(2.0f,5.0f));
    path.cubicTo(p(2.0f,3.895431f),p(2.895431f,3.0f),p(4.0f,3.0f));
    path.lineTo(p(7.9f,3.0f));
    path.cubicTo(p(8.579667f,2.993336f),p(9.216198f,3.332317f),p(9.59f,3.9f));
    path.lineTo(p(10.4f,5.1f));
    path.cubicTo(p(10.76992f,5.66172f),p(11.39741f,5.999889f),p(12.07f,6.0f));
    path.lineTo(p(20.0f,6.0f));
    path.cubicTo(p(21.10457f,6.0f),p(22.0f,6.895431f),p(22.0f,8.0f));
    path.lineTo(p(22.0f,18.0f));
    path.cubicTo(p(22.0f,19.10457f),p(21.10457f,20.0f),p(20.0f,20.0f));
    path.lineTo(p(4.0f,20.0f));
    path.cubicTo(p(2.895431f,20.0f),p(2.0f,19.10457f),p(2.0f,18.0f));
    path.lineTo(p(2.0f,17.0f));
    path.moveTo(p(2.0f,13.0f));
    path.lineTo(p(12.0f,13.0f));
    path.moveTo(p(9.0f,16.0f));
    path.lineTo(p(12.0f,13.0f));
    path.lineTo(p(9.0f,10.0f));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineColumns2: {
    canvas.strokeRect(r(3,3,18,18),color,2.0f*rect.width/24.0f,stroke);
    CanvasPath path;
    path.moveTo(p(12.0f,3.0f));
    path.lineTo(p(12.0f,21.0f));
    canvas.strokePath(path,color,stroke,true);return true;
}
case IconSymbol::OutlineMonitorPlay: {
    canvas.strokeRect(r(2,3,20,14),color,2.0f*rect.width/24.0f,stroke);
    CanvasPath path;
    path.moveTo(p(10.0f,7.75f));
    path.cubicTo(p(10.00051f,7.478765f),p(10.14742f,7.228938f),p(10.38421f,7.096651f));
    path.cubicTo(p(10.621f,6.964365f),p(10.91076f,6.970234f),p(11.142f,7.112f));
    path.lineTo(p(14.806f,9.361f));
    path.cubicTo(p(15.02805f,9.497445f),p(15.16334f,9.73938f),p(15.16334f,10.0f));
    path.cubicTo(p(15.16334f,10.26062f),p(15.02805f,10.50255f),p(14.806f,10.639f));
    path.lineTo(p(11.142f,12.889f));
    path.cubicTo(p(10.91046f,13.03095f),p(10.62026f,13.03664f),p(10.38334f,12.90386f));
    path.cubicTo(p(10.14641f,12.77108f),p(9.999784f,12.52059f),p(10.0f,12.249f));
    path.lineTo(p(10.0f,7.75f));
    path.close();
    path.moveTo(p(12.0f,17.0f));
    path.lineTo(p(12.0f,21.0f));
    path.moveTo(p(8.0f,21.0f));
    path.lineTo(p(16.0f,21.0f));
    canvas.strokePath(path,color,stroke,true);return true;
}
