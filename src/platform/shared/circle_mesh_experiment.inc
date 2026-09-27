// Included only by the opt-in performance-lab renderer build.
struct CircleVertex {
    SkPoint position;
    SkPoint offset;
    float outerRadius;
    Color color;
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
                color=half4(v.ink.rgb*v.ink.a, v.ink.a)*coverage;
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
    scratch.clear(); scratch.reserve(count*6);
    SkRect bounds = SkRect::MakeEmpty();
    constexpr SkPoint corners[] = {{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
    for (std::size_t i=0; i<count; ++i) {
        const auto r = SkRect::MakeXYWH(rects[i].rect.x,rects[i].rect.y,
                                      rects[i].rect.width,rects[i].rect.height);
        const auto center = matrix.mapPoint({r.centerX(),r.centerY()});
        const float radius = matrix.mapRadius(r.width()/2)+.5f;
        if (!std::isfinite(center.x()) || !std::isfinite(center.y()) || !std::isfinite(radius))
            return {false, "non-finite-device-geometry", 0};
        const auto b = SkRect::MakeLTRB(center.x()-radius,center.y()-radius,
                                      center.x()+radius,center.y()+radius);
        bounds.join(b);
        for (const auto corner : corners)
            scratch.push_back({{center.x()+corner.x()*radius,center.y()+corner.y()*radius},
                               corner,radius,rects[i].color});
    }
    if (!bounds.isFinite()) return {false, "non-finite-device-bounds", 0};
    auto buffer = SkMeshes::MakeVertexBuffer(scratch.data(),scratch.size()*sizeof(CircleVertex));
    auto result = SkMesh::Make(spec,SkMesh::Mode::kTriangles,std::move(buffer),scratch.size(),0,
                               nullptr,{},bounds);
    if (!result.mesh.isValid()) return {false, "mesh-validation-failed", 0};
    SkPaint paint; paint.setColor(SK_ColorWHITE);
    // Positions are in device space; existing device clip remains in effect.
    canvas.save(); canvas.resetMatrix();
    canvas.drawMesh(result.mesh,SkBlender::Mode(SkBlendMode::kDst),paint);
    canvas.restore();
    return Result{true,"mesh",scratch.size()};
}
