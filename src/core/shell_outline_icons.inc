// Exact 24-unit Lucide SVG geometry from the approved iShell prototype DOM.
// Source: http://127.0.0.1:8779/index.html#terminal (2026-10-30).
    case IconSymbol::OutlineDoubleChevronRight: {
        CanvasPath path;
        path.moveTo(p(6,17));path.lineTo(p(11,12));path.lineTo(p(6,7));
        path.moveTo(p(13,17));path.lineTo(p(18,12));path.lineTo(p(13,7));
        canvas.strokePath(path,color,2.4f*rect.width/24,true);return true; }
    case IconSymbol::OutlineCircleUser: {
        CanvasPath path;
        path.moveTo(p(18,20));path.cubicTo(p(18,16.6863f),p(15.3137f,14),p(12,14));
        path.cubicTo(p(8.6863f,14),p(6,16.6863f),p(6,20));
        canvas.strokePath(path,color,1.75f*rect.width/24,true);
        canvas.strokeEllipse(r(8,6,8,8),color,1.75f*rect.width/24);
        canvas.strokeEllipse(r(2,2,20,20),color,1.75f*rect.width/24);return true; }
    case IconSymbol::OutlineNotebook: {
        CanvasPath path;
        for(float y : {6.f,10.f,14.f,18.f}){path.moveTo(p(2,y));path.lineTo(p(6,y));}
        path.moveTo(p(16,2));path.lineTo(p(16,22));
        canvas.strokePath(path,color,1.7f*rect.width/24,true);
        canvas.strokeRect(r(4,2,16,20),color,2*rect.width/24,1.7f*rect.width/24);return true; }
    case IconSymbol::OutlineChecklist: {
        CanvasPath path;
        path.moveTo(p(3,17));path.lineTo(p(5,19));path.lineTo(p(9,15));
        path.moveTo(p(3,7));path.lineTo(p(5,9));path.lineTo(p(9,5));
        for(float y : {6.f,12.f,18.f}){path.moveTo(p(13,y));path.lineTo(p(21,y));}
        canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
    case IconSymbol::OutlineCodeBrackets: {
        CanvasPath path;
        path.moveTo(p(16,18));path.lineTo(p(22,12));path.lineTo(p(16,6));
        path.moveTo(p(8,6));path.lineTo(p(2,12));path.lineTo(p(8,18));
        canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
    case IconSymbol::OutlineSquareTerminal: {
        CanvasPath path;
        path.moveTo(p(7,11));path.lineTo(p(9,9));path.lineTo(p(7,7));
        path.moveTo(p(11,13));path.lineTo(p(15,13));
        canvas.strokePath(path,color,1.7f*rect.width/24,true);
        canvas.strokeRect(r(3,3,18,18),color,2*rect.width/24,1.7f*rect.width/24);return true; }

    case IconSymbol::FilledStar:
    case IconSymbol::OutlineStar: {
        CanvasPath path;path.moveTo(p(12,3));path.lineTo(p(14.8f,8.7f));path.lineTo(p(21.1f,9.6f));path.lineTo(p(16.55f,14.04f));path.lineTo(p(17.63f,20.3f));path.lineTo(p(12,17.34f));path.lineTo(p(6.37f,20.3f));path.lineTo(p(7.45f,14.04f));path.lineTo(p(2.9f,9.6f));path.lineTo(p(9.2f,8.7f));path.close();if(symbol==IconSymbol::FilledStar)canvas.fillPathLinearGradient(path,rect,color,color);else canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
    case IconSymbol::OutlineClock: {
        canvas.strokeEllipse(r(2,2,20,20),color,1.7f*rect.width/24);CanvasPath path;path.moveTo(p(12,6));path.lineTo(p(12,12));path.lineTo(p(16,14));canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
    case IconSymbol::OutlineCircleSlash: {
        canvas.strokeEllipse(r(3,3,18,18),color,1.7f*rect.width/24);CanvasPath path;path.moveTo(p(9,15));path.lineTo(p(15,9));canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
    case IconSymbol::OutlineHash: {
        CanvasPath path;path.moveTo(p(4,9));path.lineTo(p(20,9));path.moveTo(p(3,15));path.lineTo(p(19,15));path.moveTo(p(10,3));path.lineTo(p(8,21));path.moveTo(p(16,3));path.lineTo(p(14,21));canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
    case IconSymbol::OutlineSortArrows: {
        CanvasPath path;path.moveTo(p(8,3));path.lineTo(p(8,21));path.moveTo(p(4,7));path.lineTo(p(8,3));path.lineTo(p(12,7));path.moveTo(p(16,21));path.lineTo(p(16,3));path.moveTo(p(12,17));path.lineTo(p(16,21));path.lineTo(p(20,17));canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
    case IconSymbol::OutlineList: {
        CanvasPath path;for(float y:{6.f,12.f,18.f}){path.moveTo(p(8,y));path.lineTo(p(21,y));path.moveTo(p(3,y));path.lineTo(p(3.01f,y));}canvas.strokePath(path,color,1.7f*rect.width/24,true);return true; }
