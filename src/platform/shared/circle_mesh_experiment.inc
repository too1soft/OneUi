// Included only by the opt-in performance-lab renderer build.
// Geometry follows the bundled Ganesh circular-rrect / circle paths. This is
// deliberately not a general Canvas replacement or a cross-backend guarantee.
struct CircleVertex {
    SkPoint position;
    SkPoint offset;
    float outerRadius;
    Color color; // 8-bit premultiplied, matching native Ganesh vertex colors.
};
static sk_sp<SkMeshSpecification> circleMeshSpecification() {
    using A = SkMeshSpecification::Attribute;
    using V = SkMeshSpecification::Varying;
    static const auto spec = [] {
        const A attributes[] = {{A::Type::kFloat2, offsetof(CircleVertex, position), SkString("pos")},
            {A::Type::kFloat2, offsetof(CircleVertex, offset), SkString("offset")},
            {A::Type::kFloat, offsetof(CircleVertex, outerRadius), SkString("radius")},
            {A::Type::kUByte4_unorm, offsetof(CircleVertex, color), SkString("ink")}};
        const V varyings[] = {{V::Type::kFloat2, SkString("offset")},
            {V::Type::kFloat, SkString("radius")}, {V::Type::kHalf4, SkString("ink")}};
        return SkMeshSpecification::Make(attributes, sizeof(CircleVertex), varyings,
            SkString(R"(Varyings main(const Attributes a) {
                Varyings v; v.position=a.pos; v.offset=a.offset;
                v.radius=a.radius; v.ink=a.ink; return v;
            })"),
            SkString(R"(float2 main(const Varyings v, out half4 color) {
                half coverage=saturate(half(v.radius*(1.0-length(v.offset))));
                color=v.ink*coverage;
                return v.position;
            })")).specification;
    }();
    return spec;
}
static experimental::CircleMeshResult drawCircleMesh(
        SkCanvas& canvas, const RoundedRectFill* rects, std::size_t count) {
    using Result = experimental::CircleMeshResult;
    if (!canvas.recordingContext()) return {false, "software-backend", 0};
    if (!count) return {true, "empty", 0};
    if (!rects || count > 16384) return {false, "unsupported-count", 0};
    const auto matrix = canvas.getTotalMatrix();
    const float scale = matrix.getScaleX();
    if (!matrix.isFinite() || !matrix.isScaleTranslate() || !std::isfinite(scale) || scale <= 0 ||
        scale != matrix.getScaleY()) return {false, "unsupported-transform", 0};
    for (std::size_t i=0; i<count; ++i) {
        const auto& r = rects[i];
        if (!std::isfinite(r.rect.x) || !std::isfinite(r.rect.y) ||
            !std::isfinite(r.rect.width) || r.rect.width <= 0 ||
            r.rect.width != r.rect.height || r.radius != r.rect.width/2)
            return {false, "non-circle", 0};
    }
    const auto spec = circleMeshSpecification();
    if (!spec) return {false, "shader-specification-failed", 0};
    // CPU-backed buffers are copied for each submission, so queued draws never
    // observe scratch data overwritten by a later frame/window.
    static thread_local std::vector<CircleVertex> scratch;
    static thread_local std::vector<std::uint16_t> indices;
    scratch.clear(); scratch.reserve(std::min(count*16,std::size_t(65536)));
    indices.clear(); indices.reserve(std::min(count,std::size_t(4096))*54);
    std::vector<SkMesh> meshes;
    meshes.reserve(4);
    std::size_t totalVertices = 0;
    SkRect bounds = SkRect::MakeEmpty();
    // Validate/own every chunk before drawing any: a rejected later item must
    // not leave a partially drawn batch underneath the caller's fallback.
    const auto finishChunk = [&] {
        if (scratch.empty()) return true;
        if (!bounds.isFinite()) return false;
        auto vb = SkMeshes::MakeVertexBuffer(scratch.data(),scratch.size()*sizeof(CircleVertex));
        auto ib = SkMeshes::MakeIndexBuffer(indices.data(),indices.size()*sizeof(std::uint16_t));
        auto result = SkMesh::MakeIndexed(spec,SkMesh::Mode::kTriangles,std::move(vb),
            scratch.size(),0,std::move(ib),indices.size(),0,nullptr,{},bounds);
        if (!result.mesh.isValid()) return false;
        totalVertices += scratch.size();
        meshes.push_back(std::move(result.mesh));
        scratch.clear(); indices.clear(); bounds = SkRect::MakeEmpty();
        return true;
    };
    constexpr float k = 0.41421356237f;
    constexpr SkPoint octagon[] = {{-k,-1},{k,-1},{1,-k},{1,k},{k,1},{-k,1},{-1,k},{-1,-k}};
    for (std::size_t i=0; i<count; ++i) {
        if (scratch.size()+16 > 65536 && !finishChunk())
            return {false,"mesh-validation-failed",0};
        const auto base = scratch.size();
        const auto triangle = [&](int a,int b,int c) {
            for (int index : {a,b,c}) indices.push_back(static_cast<std::uint16_t>(base+index));
        };
        const auto r = SkRect::MakeXYWH(rects[i].rect.x,rects[i].rect.y,
                                      rects[i].rect.width,rects[i].rect.height);
        if (!r.isFinite() || r.isEmpty()) return {false,"unsupported-rounded-bounds",0};
        const auto rr = SkRRect::MakeRectXY(r,rects[i].radius,rects[i].radius);
        const auto source = rects[i].color;
        const auto premul = [a=unsigned(source.a)](unsigned c) {
            return static_cast<unsigned char>((c*a+127)/255);
        };
        const Color ink{premul(source.r),premul(source.g),premul(source.b),source.a};
        // MakeRectXY can leave a tiny interior strip when x + width rounds.
        // A mathematical circle quad loses that native rasterization behavior.
        if (rr.isOval()) {
            if (!SkScalarNearlyEqual(r.width(),r.height()))
                return {false,"unsupported-rounded-oval",0};
            auto center = matrix.mapPoint({r.centerX(),r.centerY()});
            const float radius = matrix.mapRadius(r.width()/2)+.5f;
            const auto b = SkRect::MakeLTRB(center.x()-radius,center.y()-radius,
                                          center.x()+radius,center.y()+radius);
            if (!b.isFinite()) return {false,"non-finite-device-geometry",0};
            bounds.join(b);
            center = {b.centerX(),b.centerY()};
            const float halfWidth = b.width()*.5f;
            for (const auto offset : octagon)
                scratch.push_back({center+offset*halfWidth,offset,radius,ink});
            scratch.push_back({center,{0,0},radius,ink});
            for (int j=0;j<8;++j) triangle(j,(j+1)%8,8);
        } else {
            auto b = matrix.mapRect(r);
            const float radius = rr.getSimpleRadii().x()*scale+.5f;
            if (radius < 1.f) return {false,"unsupported-small-radius",0};
            b.outset(.5f,.5f);
            if (!b.isFinite() || !std::isfinite(radius))
                return {false,"non-finite-device-geometry",0};
            bounds.join(b);
            const float xs[] = {b.left(),b.left()+radius,b.right()-radius,b.right()};
            const float ys[] = {b.top(),b.top()+radius,b.bottom()-radius,b.bottom()};
            const float offsets[] = {-1,0,0,1};
            for (int y=0;y<4;++y) for (int x=0;x<4;++x)
                scratch.push_back({{xs[x],ys[y]},{offsets[x],offsets[y]},radius,ink});
            // Corners, edges, then center: preserve the native nine-patch order,
            // including subpixel-width interior strips after float rounding.
            constexpr int cells[] = {0,2,8,10,1,4,6,9,5};
            for (int cell : cells) {
                triangle(cell,cell+1,cell+5);
                triangle(cell,cell+5,cell+4);
            }
        }
    }
    if (!finishChunk()) return {false,"mesh-validation-failed",0};
    SkPaint paint; paint.setColor(SK_ColorWHITE);
    // Positions are in device space; existing device clip remains in effect.
    canvas.save(); canvas.resetMatrix();
    for (const auto& mesh : meshes) canvas.drawMesh(mesh,SkBlender::Mode(SkBlendMode::kDst),paint);
    canvas.restore();
    return Result{true,"mesh",totalVertices,meshes.size()};
}
